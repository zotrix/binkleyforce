/*
 *	binkleyforce -- unix FTN mailer project
 *	
 *	Copyright (c) 1998-2000 Alexander Belkin, 2:5020/1398.11
 *	
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *	
 *	$Id$
 */

#include "includes.h"
#include "confread.h"
#include "logger.h"
#include "util.h"
#include "session.h"
#include "outbound.h"
#include "io.h"
#include "prot_common.h"

typedef enum hydra_pkttype {
	HPKT_EXIT      = -3,
	HPKT_JUNK      = -2,
	HPKT_NONE      = -1,
	HPKT_START     = 'A',
	HPKT_INIT      = 'B',
	HPKT_INITACK   = 'C',
	HPKT_FINFO     = 'D',
	HPKT_FINFOACK  = 'E',
	HPKT_DATA      = 'F',
	HPKT_DATAACK   = 'G',
	HPKT_RPOS      = 'H',
	HPKT_EOF       = 'I',
	HPKT_EOFACK    = 'J',
	HPKT_END       = 'K',
	HPKT_IDLE      = 'L',
	HPKT_DEVDATA   = 'M',
	HPKT_DEVDACK   = 'N',
	HPKT_MIN       = HPKT_START,
	HPKT_MAX       = HPKT_DEVDACK
} e_hydra_pkttype;

typedef enum hydra_char {
	HCHR_PKTEND    = 'a',
	HCHR_BINPKT    = 'b',
	HCHR_HEXPKT    = 'c',
	HCHR_ASCPKT    = 'd',
	HCHR_UUEPKT    = 'e',
	HCHR_MIN       = HCHR_PKTEND,
	HCHR_MAX       = HCHR_UUEPKT
} e_hydra_char;

typedef enum hydra_txstate {
	HTX_DONE,
	HTX_START,
	HTX_SWAIT,
	HTX_INIT,
	HTX_INITACK,
	HTX_RINIT,
	HTX_NextFile,
	HTX_ToFName,
	HTX_FINFO,
	HTX_FINFOACK,
	HTX_DATA,
	HTX_SkipFile,
	HTX_DATAACK,
	HTX_XWAIT,
	HTX_EOF,
	HTX_EOFACK,
	HTX_REND,
	HTX_END,
	HTX_ENDACK,
	HTX_Abort
} e_hydra_txstate;

typedef enum hydra_rxstate {
	HRX_DONE,
	HRX_INIT,
	HRX_FINFO,
	HRX_DATA,
	HRX_RPOS,
	HRX_RPosAck,
	HRX_Skip,
	HRX_SkipAck
} e_hydra_rxstate;

char *hydra_pkttype_names[] =
{
	"HPKT_START",
	"HPKT_INIT",
	"HPKT_INITACK",
	"HPKT_FINFO",
	"HPKT_FINFOACK",
	"HPKT_DATA",
	"HPKT_DATAACK",
	"HPKT_RPOS",
	"HPKT_EOF",
	"HPKT_EOFACK",
	"HPKT_END",
	"HPKT_IDLE",
	"HPKT_DEVDATA"
};

char *hydra_char_names[] =
{
	"HCHR_PKTEND",
	"HCHR_BINPKT",
	"HCHR_HEXPKT",
	"HCHR_ASCPKT",
	"HCHR_UUEPKT"
};

char *hydra_txstate_names[] =
{
	"HTX_DONE",
	"HTX_START",
	"HTX_SWAIT",
	"HTX_INIT",
	"HTX_INITACK",
	"HTX_RINIT",
	"HTX_NextFile",
	"HTX_ToFName",
	"HTX_FINFO",
	"HTX_FINFOACK",
	"HTX_DATA",
	"HTX_SkipFile",
	"HTX_DATAACK",
	"HTX_XWAIT",
	"HTX_EOF",
	"HTX_EOFACK",
	"HTX_REND",
	"HTX_END",
	"HTX_ENDACK",
	"HTX_Abort"
};

char *hydra_rxstate_names[] =
{
	"HRX_DONE",
	"HRX_INIT",
	"HRX_FINFO",
	"HRX_DATA",
	"HRX_RPOS",
	"HRX_RPosAck",
	"HRX_Skip",
	"HRX_SkipAck"
};

#define HYDRA_DLE		('X' - '@')
#define HYDRA_MINBLKLEN		64
#define HYDRA_MAXBLKLEN		2048
#define HYDRA_OVERHEAD		8
#define HYDRA_MAXDATALEN	(HYDRA_MAXBLKLEN + HYDRA_OVERHEAD + 5)
#define HYDRA_MAXPKTLEN		((HYDRA_MAXBLKLEN + HYDRA_OVERHEAD + 5) * 3)
#define HYDRA_BUFLEN		(HYDRA_MAXPKTLEN + 16)
#define HYDRA_PKTPREFIX		31

#define HOPT_XONXOFF		0x00000001L
#define HOPT_TELENET		0x00000002L
#define HOPT_CTLCHRS		0x00000004L
#define HOPT_HIGHCTL		0x00000008L
#define HOPT_HIGHBIT		0x00000010L
#define HOPT_CANBRK		0x00000020L
#define HOPT_CANASC		0x00000040L
#define HOPT_CANUUE		0x00000080L
#define HOPT_CRC32		0x00000100L
#define HOPT_DEVICE		0x00000200L
#define HOPT_FPT		0x00000400L

/* What we can do */
#define HYDRA_CANOPT		(HOPT_XONXOFF | HOPT_TELENET | HOPT_CTLCHRS | HOPT_HIGHCTL | HOPT_HIGHBIT | HOPT_CANASC | HOPT_CANUUE | HOPT_CRC32)
/* Vital options if we ask for any; abort if other side doesn't support them */
#define HYDRA_NECOPT		(HOPT_XONXOFF | HOPT_TELENET | HOPT_CTLCHRS | HOPT_HIGHCTL | HOPT_HIGHBIT | HOPT_CANBRK)
/* Default options */
#define HYDRA_DEFOPT		(0L | HOPT_CRC32)
/* rxoptions during init (needs to handle ANY link yet unknown at that point */
#define HYDRA_RXIOPT		(HOPT_XONXOFF | HOPT_TELENET | HOPT_CTLCHRS | HOPT_HIGHCTL | HOPT_HIGHBIT)
/* ditto, but this time txoptions */
#define HYDRA_TXIOPT		(HOPT_XONXOFF | HOPT_TELENET | HOPT_CTLCHRS | HOPT_HIGHCTL | HOPT_HIGHBIT)

#define HYDRA_UUENC(c)		(((c) & 0x3f) + '!')
#define HYDRA_UUDEC(c)		(((c) - '!') & 0x3f)
#define HYDRA_ISUUE(c)		((c) > ' ' && (c) < 'a')


#ifdef DEBUG
#define HYDRA_TXSTATE(c)	(hydra_txstate_names[c])
#define HYDRA_RXSTATE(c)	(hydra_rxstate_names[c])
#define HYDRA_PKTNAME(c)	((c < HPKT_MIN || c > HPKT_MAX) ? "Out of range" : hydra_pkttype_names[c - HPKT_MIN])
#define HYDRA_CHRNAME(c)	((c < HCHR_MIN || c > HCHR_MAX) ? "Out of range" : hydra_char_names[c - HCHR_MIN])
#else
#define HYDRA_TXSTATE(c)	("No debug information")
#define HYDRA_RXSTATE(c)	("No debug information")
#define HYDRA_PKTNAME(c)	("No debug information")
#define HYDRA_CHRNAME(c)	("No debug information")
#endif

typedef struct {
	const char *str;
	unsigned long val;
	size_t len;
} s_hydraflag;

typedef struct {
	bool sent;
	enum hydra_pkttype type;
	char *data;
	size_t size;
} s_hydrapkt;

s_hydraflag hydraflags[] =
{
	{ "XON", HOPT_XONXOFF, 3 },
	{ "TLN", HOPT_TELENET, 3 },
	{ "CTL", HOPT_CTLCHRS, 3 },
	{ "HIC", HOPT_HIGHCTL, 3 },
	{ "HI8", HOPT_HIGHBIT, 3 },
	{ "BRK", HOPT_CANBRK,  3 },
	{ "ASC", HOPT_CANASC,  3 },
	{ "UUE", HOPT_CANUUE,  3 },
	{ "C32", HOPT_CRC32,   3 },
	{ "DEV", HOPT_DEVICE,  3 },
	{ "FPT", HOPT_FPT,     3 },
	{ NULL , 0x0L,         0 }
};

typedef struct {
	char *oencbuf;		/* Encoded data for sending */
	char *oencptr;		/* Pointer to the UNSENT encoded data */
	int   oencpos;		/* Total length of encoded data */
	char *obuf;
	int   lastchar;		/* Last character we sent */
	
	char *iencbuf;
	char *iencptr;
	bool  iencrcv;		/* Flag: we are receiving packet now */
	int   iencfmt;		/* Format of packet we are receiving */
	int   idlecnt;		/* Counter for received HYDRA_DLE characters */
	char *ibuf;		/* Decoded packet */
	int   isize;		/* Decoded packet size */
	
	int timeout_send;
	int timeout_recv;
	int timeout_dead;

	int send_block_size;
	int send_good_blocks; /* Count sent blocks for current block size */
	int recv_block_size;
	
	unsigned long txwindow;
	unsigned long rxwindow;

	unsigned long options;
	unsigned long rxoptions;
	unsigned long txoptions;

	long recv_rpos_id;
	long send_rpos_id;
	
	bool batchesdone;	/* It is not our first batch? */
	
	s_hydrapkt *pktqueue;
	int n_pkts;
} s_hydrainfo;

static char *hydra_putlong(char *buf, long val)
{
	buf[0] = ( ((unsigned long) val)       ) & 0xff;
	buf[1] = ( ((unsigned long) val) >> 8  ) & 0xff;
	buf[2] = ( ((unsigned long) val) >> 16 ) & 0xff;
	buf[3] = ( ((unsigned long) val) >> 24 ) & 0xff;
	
	return buf;
}

static char *hydra_putword(char *buf, int val)
{
	buf[0] = ( ((unsigned int) val)       ) & 0xff;
	buf[1] = ( ((unsigned int) val) >> 8  ) & 0xff;
	
	return buf;
}

static long hydra_getlong(const char *buf)
{
	return ( (unsigned long) ((unsigned char) buf[0])       )
	     | ( (unsigned long) ((unsigned char) buf[1]) << 8  )
	     | ( (unsigned long) ((unsigned char) buf[2]) << 16 )
	     | ( (unsigned long) ((unsigned char) buf[3]) << 24 );
}

static int hydra_getword(const char *buf)
{
	return ( (unsigned int) ((unsigned char) buf[0])      )
	     | ( (unsigned int) ((unsigned char) buf[1]) << 8 );
}

static char *hydra_putflags(char *buf, unsigned long flags, size_t szbuf)
{
	int i;
	size_t szused = 0;

	*buf = '\0';
	
	for( i = 0; hydraflags[i].str; i++ )
	{
		if( hydraflags[i].val & flags )
		{
			if( szused + hydraflags[i].len + 2 > szbuf )
			{
				ASSERT_MSG();
				break;
			}
			else if( szused > 0 )
			{
				*buf++ = ',';
				++szused;
			}
			strcpy(buf, hydraflags[i].str);
			buf += hydraflags[i].len;
			szused += hydraflags[i].len;
		}
	}

	return buf;
}

static unsigned long hydra_getflags(char *buf)
{
	int i;
	char *p;
	unsigned long result = 0L;
	
	for( p = strtok(buf, ","); p; p = strtok(NULL, ",") )
	{
		for( i = 0; hydraflags[i].str; i++ )
		{
			if( strcmp(p, hydraflags[i].str) == 0 )
			{
				result |= hydraflags[i].val;
				break;
			}
		}
	}
	
	return result;
}

#ifdef DEBUG
static void hydra_pktdebug(bool send, s_hydrainfo *hi, const char *data,
                           e_hydra_pkttype pkttype, e_hydra_char format,
                           bool crc32, size_t sz)
{
	char *p;
	
	DEB((D_PROT, "hydra_pktdebug: %s packet '%s' #%d (len=%ld, format='%s', crc=%d)",
		send ? "<-" : "->", HYDRA_PKTNAME(pkttype), pkttype, sz,
		HYDRA_CHRNAME(format), crc32 ? 32 : 16));
	
	/*
	 * For some packet types we will log more information
	 */
	switch(pkttype) {
	case HPKT_FINFO:
		DEB((D_PROT, "hydra_pktdebug:    FINFO \"%s\"",
			p = string_printable_buffer(data, sz)));
		if( p ) free(p);
		break;
	case HPKT_FINFOACK:
		if( data[0] == '\0' )
			DEB((D_PROT, "hydra_pktdebug:    FINFOACK (End Of Batch)"));
		else
			DEB((D_PROT, "hydra_pktdebug:    FINFOACK (offset=%ld)",
				(long)hydra_getlong(data)));
		break;
	case HPKT_DATA:
		DEB((D_PROT, "hydra_pktdebug:    DATA (offset=%ld, length=%ld)",
			(long)hydra_getlong(data), (long)(sz - 5)));
		break;
	case HPKT_DATAACK:
		DEB((D_PROT, "hydra_pktdebug:    DATAACK (offset=%ld)",
			hydra_getlong(data)));
		break;
	case HPKT_RPOS:
		DEB((D_PROT, "hydra_pktdebug:    RPOS (pos=%ld, blklen=%d, syncid=%ld)",
			hydra_getlong(data), hydra_getword(data+4),
			hydra_getlong(data+6)));
		break;
	case HPKT_EOF:
		DEB((D_PROT, "hydra_pktdebug:    EOF (offset=%ld)",
			hydra_getlong(data)));
		break;
	default:
		/* Avoid warnings about unhandled values */
		break;
	}
}
#endif /* DEBUG */

static char *hydra_putbinbyte(s_hydrainfo *hi, char *buf, unsigned char c)
{
	unsigned char n = c;

	if( hi->txoptions & HOPT_HIGHCTL )
		n &= 0x7f;
	
	if( (n == HYDRA_DLE)
	 || ((hi->txoptions & HOPT_XONXOFF) && ((n == XON) || (n == XOFF)))
	 || ((hi->txoptions & HOPT_TELENET) && (n == '\r') && (hi->lastchar == '@'))
	 || ((hi->txoptions & HOPT_CTLCHRS) && ((n < 32) || (n == 127))) )
	{
		*buf++ = HYDRA_DLE;
		c ^= 0x40;
	}
	
	*buf++ = c;
	hi->lastchar = n;
	
	return buf;
}

static char *hydra_puthexbyte(char *buf, unsigned char c)
{
	static const char hexdigit[] = "0123456789abcdef";
	
	if( c & 0x80 )
	{
		*buf++ = '\\';
		*buf++ = hexdigit[(c >> 4) & 0x0f];
		*buf++ = hexdigit[(c     ) & 0x0f];
	}
	else if( (c < 32) || (c == 127) )
	{
		*buf++ = HYDRA_DLE;
		*buf++ = c ^ 0x40;
	}
	else if( c == '\\' )
	{
		*buf++ = '\\';
		*buf++ = '\\';
	}
	else
		*buf++ = c;
	
	return buf;
}

static char *hydra_putascblock(s_hydrainfo *hi, char *buf, char *src, size_t szsrc)
{
	int ch = 0;
	int bitshift = 0;

	for( ; szsrc > 0; --szsrc, ++src )
	{
		ch |= ((unsigned char)(*src) << bitshift);
		
		buf = hydra_putbinbyte(hi, buf, ((unsigned char)(ch) & 0x7f));
		ch >>= 7;
		
		if( ++bitshift >= 7 )
		{
			buf = hydra_putbinbyte(hi, buf, ((unsigned char)(ch) & 0x7f));
			ch = bitshift = 0;
		}
	}
	
	if( bitshift > 0 )
		buf = hydra_putbinbyte(hi, buf, ((unsigned char)(ch) & 0x7f));
	
	return buf;
}

static char *hydra_putuueblock(char *buf, char *src, size_t szsrc)
{
	for( ; szsrc >= 3; src += 3, szsrc -= 3 )
	{
		*buf++ = HYDRA_UUENC(src[0] >> 2);
		*buf++ = HYDRA_UUENC(((src[0] << 4) & 0x30) | ((src[1] >> 4) & 0x0f));
		*buf++ = HYDRA_UUENC(((src[1] << 2) & 0x3c) | ((src[2] >> 6) & 0x03));
		*buf++ = HYDRA_UUENC(src[2] & 0x3f);
	}
	
	if( szsrc > 0 )
	{
		*buf++ = HYDRA_UUENC(src[0] >> 2);
		*buf++ = HYDRA_UUENC(((src[0] << 4) & 0x30) | ((src[1] >> 4) & 0x0f));
		if( szsrc == 2 )
			*buf++ = HYDRA_UUENC((src[1] << 2) & 0x3c);
	}
	
	return buf;
}

static char *hydra_putpkt(s_hydrainfo *hi, char *dst, char *src, size_t szsrc,
                          enum hydra_pkttype pkttype)
{
	enum hydra_char format;
	char *obuf;
	int pos;
	
	switch(pkttype) {
	case HPKT_START:
	case HPKT_INIT:
	case HPKT_INITACK:
	case HPKT_END:
	case HPKT_IDLE:
		format = HCHR_HEXPKT;
		break;
	default:
		if( hi->txoptions & HOPT_HIGHBIT )
		{
			if( (hi->txoptions & HOPT_CTLCHRS)
			 && (hi->txoptions & HOPT_CANUUE ) )
				format = HCHR_UUEPKT;
			else if( hi->txoptions & HOPT_CANASC )
				format = HCHR_ASCPKT;
			else
				format = HCHR_HEXPKT;
		}
		else
			format = HCHR_BINPKT;
	} /* end of switch */

#ifdef DEBUG
	hydra_pktdebug(TRUE, hi, src, pkttype, format,
		(format != HCHR_HEXPKT && (hi->txoptions & HOPT_CRC32)), szsrc);
#endif

	src[szsrc++] = pkttype;
	
	if( format != HCHR_HEXPKT && (hi->txoptions & HOPT_CRC32) )
	{
		unsigned long crc32 = ~getcrc32ccitt(src, szsrc);
		
		src[szsrc++] = (((unsigned char) (crc32      )) & 0xff);
		src[szsrc++] = (((unsigned char) (crc32 >> 8 )) & 0xff);
		src[szsrc++] = (((unsigned char) (crc32 >> 16)) & 0xff);
		src[szsrc++] = (((unsigned char) (crc32 >> 24)) & 0xff);
	}
	else
	{
		unsigned short crc16 = ~getcrc16ccitt(src, szsrc);
		
		src[szsrc++] = (((unsigned char) (crc16     )) & 0xff);
		src[szsrc++] = (((unsigned char) (crc16 >> 8)) & 0xff);
	}
	
	obuf = dst;
	*obuf++ = HYDRA_DLE;
	*obuf++ = format;
	
	switch(format) {
	case HCHR_BINPKT:
		for( pos = 0; pos < szsrc; pos++ )
			obuf = hydra_putbinbyte(hi, obuf, *src++);
		break;
	case HCHR_HEXPKT:
		for( pos = 0; pos < szsrc; pos++ )
			obuf = hydra_puthexbyte(obuf, *src++);
		break;
	case HCHR_ASCPKT:
		obuf = hydra_putascblock(hi, obuf, src, szsrc);
		break;
	case HCHR_UUEPKT:
		obuf = hydra_putuueblock(obuf, src, szsrc);
		break;
	default:
		ASSERT(0);
	}
	
	*obuf++ = HYDRA_DLE;
	*obuf++ = HCHR_PKTEND;
	
	if( pkttype != HPKT_DATA && format != HCHR_BINPKT )
	{
		*obuf++ = '\r';
		*obuf++ = '\n';
	}
	
	return obuf;
}

void hydra_queuepkt(s_hydrainfo *hi, char *data, size_t szdata,
                    enum hydra_pkttype pkttype)
{
	char *endptr = NULL;
	char *buffer = NULL;
	size_t pktlen;

	ASSERT(szdata <= HYDRA_MAXBLKLEN);
	
	if( data && szdata > 0 )
	{
		buffer = xmalloc(HYDRA_BUFLEN);
		endptr = hydra_putpkt(hi, buffer, data, szdata, pkttype);
		pktlen = endptr -  buffer;
	
		/* Check for buffer overflow */
		ASSERT(pktlen <= HYDRA_BUFLEN);
	
		/* Release unused memory */
		buffer = xrealloc(buffer, endptr - buffer);
	}
	else
	{
		buffer = xmalloc(HYDRA_OVERHEAD*3);
		endptr = hydra_putpkt(hi, buffer, hi->obuf, 0, pkttype);
		pktlen = endptr -  buffer;
		
		/* Check for buffer overflow */
		ASSERT(pktlen <= (HYDRA_OVERHEAD*3));
	}
	
	/* Add new entry to the queue */
	hi->pktqueue = xrealloc(hi->pktqueue, sizeof(s_hydrapkt)*(hi->n_pkts + 1));
	memset(&hi->pktqueue[hi->n_pkts], '\0', sizeof(s_hydrapkt));
	
	/* Set packet data */
	hi->pktqueue[hi->n_pkts].sent = FALSE;
	hi->pktqueue[hi->n_pkts].type = pkttype;
	hi->pktqueue[hi->n_pkts].data = buffer;
	hi->pktqueue[hi->n_pkts].size = pktlen;
	hi->n_pkts++;
	
	DEB((D_PROT, "hydra_queuepkt: queue packet '%s' #%d, %ld bytes",
		HYDRA_PKTNAME(pkttype), (int)pkttype, (long)(endptr - buffer)));
}

static int hydra_buffer_packet(s_hydrainfo *hi, s_hydrapkt *pkt)
{
	/* Is there space for the new pkt? */
	if( hi->oencpos + pkt->size > HYDRA_BUFLEN ) return 1;
	
	if( pkt->data )
	{
		DEB((D_PROT, "hydra_buffer_packet: buffer packet '%s' #%d, %ld bytes",
			HYDRA_PKTNAME(pkt->type), (int)pkt->type, (long)pkt->size));
		memcpy(hi->oencbuf + hi->oencpos, pkt->data, pkt->size);
		hi->oencpos += pkt->size;
		free(pkt->data);
		pkt->data = NULL;
	}
	
	return 0;
}

static int hydra_send(s_hydrainfo *hi)
{
	int i;

	if( hi->oencpos == 0 && hi->pktqueue )
	{
		/*
		 *  Buffer is empty and there are unsent packets
		 */
		for( i = 0; i < hi->n_pkts; i++ )
		{
			if( hi->pktqueue[i].sent == FALSE )
			{
				if( hydra_buffer_packet(hi, &hi->pktqueue[i]) )
					break;
				else
					hi->pktqueue[i].sent = TRUE;
			}
		}
		/* If the packet queue is empty, free it */
		if( i >= hi->n_pkts )
		{
			if( hi->pktqueue )
			{
				free(hi->pktqueue);
				hi->pktqueue = NULL;
			}
			hi->n_pkts = 0;
		}
	}
	
	/* Send buffered data */
	if( hi->oencpos )
	{
		int n = tty_write(hi->oencptr, hi->oencpos);
		
		if( n >= hi->oencpos )
		{
			hi->oencptr = hi->oencbuf;
			hi->oencpos = 0;
		}
		else if( n > 0 )
		{
			hi->oencptr += n;
			hi->oencpos -= n;
		}
		else
			return -1;
	}
	
	return 0;
}

static char *hydra_getbinblock(char *dst, size_t szdst, char *src, size_t szsrc)
{
	if( szsrc > szdst )
	{
		log("Hydra: BIN packet buffer overflow detected");
		return NULL;
	}
	
	memcpy(dst, src, szsrc);

	return dst + szsrc;
}

static char *hydra_gethexblock(char *dst, size_t szdst, char *src, size_t szsrc)
{
	char *in  = src;
	char *out = dst;
	char *enddst = dst + szdst - 1;
	char *endsrc = src + szsrc - 1;
	int hex1 = 0;
	int hex2 = 0;

	while( in <= endsrc && out <= enddst )
	{
		if( (*in == '\\') && (*++in != '\\') )
		{
			hex1 = (unsigned char)(*in++);
			hex2 = (unsigned char)(*in++);
			
			if( (hex1 -= '0') > 9 )
				hex1 -= ('a' - ':');
			if( (hex2 -= '0') > 9 )
				hex2 -= ('a' - ':');
			
			*out++ = ((hex1 << 4) | hex2) & 0xff;
		}
		else
			*out++ = *in++;
	}
	
	if( out > enddst )
		log("Hydra: HEX decoder buffer overflow detected");
	
	return out;
}

static char *hydra_getascblock(char *dst, size_t szdst, char *src, size_t szsrc)
{
	char *in  = src;
	char *out = dst;
	char *enddst = dst + szdst - 1;
	char *endsrc = src + szsrc - 1;
	int n = 0;
	int c = 0;
	
	while( (in <= endsrc) && (out <= enddst) )
	{
		c |= (((unsigned char)(*in) & 0x7f) << n);

		if( (n += 7) >= 8 )
		{
			*out++ = (unsigned char)(c & 0xff);
			c >>= 8;
			n -= 8;
		}
		++in;
	}

	if( out > enddst )
		log("Hydra: ASC decoder buffer overflow detected");
	
	return out;
}

static char *hydra_getuueblock(char *dst, size_t szdst, char *src, size_t szsrc)
{
	char *in  = src;
	char *out = dst;

	/*
	 * Check UUE block size
	 */
	if( (szsrc % 4) == 1 )
	{
		log("Hydra: not enough characters in UUE block");
		return NULL;
	}

	/*
	 * Check destination buffer size to avoid overflows
	 */
	if( ((szsrc / 4) * 4 + (szsrc % 4)) > szdst )
	{
		log("Hydra: buffer overflow in UU decoder");
		return NULL;
	}
	
	/*
	 * UU decoder
	 */
	while( szsrc >= 4 )
	{
		if( !HYDRA_ISUUE(in[0]) || !HYDRA_ISUUE(in[1])
		 || !HYDRA_ISUUE(in[2]) || !HYDRA_ISUUE(in[3]) )
		{
			log("Hydra: character out of range in UUE block");
			return NULL;
		}
		
		*out++ = (unsigned char)((HYDRA_UUDEC(in[0]) << 2)
		                       | (HYDRA_UUDEC(in[1]) >> 4));
		*out++ = (unsigned char)((HYDRA_UUDEC(in[1]) << 4)
		                       | (HYDRA_UUDEC(in[2]) >> 2));
		*out++ = (unsigned char)((HYDRA_UUDEC(in[2]) << 6)
		                       | (HYDRA_UUDEC(in[3])     ));
		
		in    += 4;
		szsrc -= 4;
	}
	
	if( szsrc >= 2 )
	{
		if( !HYDRA_ISUUE(in[0]) || !HYDRA_ISUUE(in[1]) )
		{
			log("Hydra: character out of range in UUE block");
			return NULL;
		}
		*out++ = (unsigned char)((HYDRA_UUDEC(in[0]) << 2)
		                       | (HYDRA_UUDEC(in[1]) >> 4));

		if( szsrc == 3 )
		{
			if( !HYDRA_ISUUE(in[2]) )
			{
				log("Hydra: character out of range in UUE block");
				return NULL;
			}
			*out++ = (unsigned char)((HYDRA_UUDEC(in[1]) << 4)
			                       | (HYDRA_UUDEC(in[2]) >> 2));
		}
	}
	
	return out;
}

static int hydra_recv(s_hydrainfo *hi)
{
	int n, c;
	char *p;
	bool goodcrc = FALSE;
	int pkttype;
	
	while( (c = GETCHAR(0)) >= 0 )
	{
		if( hi->rxoptions & HOPT_HIGHBIT )
			c &= 0x7f;

		n = c;
		if( hi->rxoptions & HOPT_HIGHCTL )
			n &= 0x7f;

		if( (n != HYDRA_DLE)
		 && (((hi->rxoptions & HOPT_XONXOFF) && ((n == XON) || (n == XOFF)))
		  || ((hi->rxoptions & HOPT_CTLCHRS) && ((n < 32  ) || (n == 127 )))) )
		{
			DEB((D_PROT, "hydra_recv: received character 0x%02x discarded", n));
			continue;
		}
		
		if( c == HYDRA_DLE )
		{
			if( ++hi->idlecnt >= 5 )
			{
				log("Hydra: got 5 DLE characters (abort)");
				return HPKT_EXIT;
			}
		}
		else if( hi->idlecnt )
		{
			hi->idlecnt = 0;
			
			switch(c) {
			case HCHR_PKTEND:
				if( hi->iencrcv )
				{
					size_t encsize = hi->iencptr - hi->iencbuf;
					
					switch(hi->iencfmt) {
					case HCHR_BINPKT:
						p = hydra_getbinblock(hi->ibuf, HYDRA_MAXDATALEN, hi->iencbuf, encsize);
						break;
					case HCHR_HEXPKT:
						p = hydra_gethexblock(hi->ibuf, HYDRA_MAXDATALEN, hi->iencbuf, encsize);
						break;
					case HCHR_ASCPKT:
						p = hydra_getascblock(hi->ibuf, HYDRA_MAXDATALEN, hi->iencbuf, encsize);
						break;
					case HCHR_UUEPKT:
						p = hydra_getuueblock(hi->ibuf, HYDRA_MAXDATALEN, hi->iencbuf, encsize);
						break;
					default:
						ASSERT(0);
					}
					
					hi->iencrcv = FALSE;
					hi->iencptr = hi->iencbuf;

					if( !p )
					{
						log("Hydra: error decoding packet format %d",
							hi->iencfmt);
						return HPKT_NONE;
					}
					
					/*
					 * Calculate decoded packet size
					 */
					hi->isize = p - hi->ibuf;

					if( (hi->iencfmt != HCHR_HEXPKT) && (hi->rxoptions & HOPT_CRC32) )
					{
						goodcrc = CRC32TEST(getcrc32ccitt(hi->ibuf, hi->isize));
						hi->isize -= 4; /* remove CRC-32 */
					}
					else
					{
						goodcrc = CRC16TEST(getcrc16ccitt(hi->ibuf, hi->isize));
						hi->isize -= 2; /* remove CRC-16 */
					}
					
					--hi->isize; /* remove packet type */
					
					if( hi->isize >= 0 )
					{
						pkttype = hi->ibuf[hi->isize];

#ifdef DEBUG
						hydra_pktdebug(FALSE, hi, hi->ibuf, pkttype, hi->iencfmt,
							(hi->iencfmt != HCHR_HEXPKT && (hi->rxoptions & HOPT_CRC32)),
							hi->isize);
#endif
						
						if( !goodcrc )
							log("Hydra: CRC test failed");
						else if( pkttype < HPKT_MIN || pkttype > HPKT_MAX )
							log("Hydra: packet type %d out of range",
								pkttype);
						else
							return pkttype;
					} else
						log("Hydra: got invalid packet (ignored)");
				} else
					log("Hydra: got unexpected packet end");
				
				DEB((D_PROT, "hydra_recv: broken packet"));

				/* Return junk error */
				return HPKT_JUNK;

			case HCHR_BINPKT:
			case HCHR_HEXPKT:
			case HCHR_ASCPKT:
			case HCHR_UUEPKT:
				if( hi->iencrcv )
					log("Hydra: packet was not received completly");
				
				hi->iencrcv = TRUE;
				hi->iencptr = hi->iencbuf;
				hi->iencfmt = c;
				
				DEB((D_PROT, "hydra_recv: start receiving '%s' (%d) packet",
					HYDRA_CHRNAME(hi->iencfmt), hi->iencfmt));
				break;

			default:
				if( hi->iencrcv )
				{
					if( hi->iencptr - hi->iencbuf < HYDRA_BUFLEN )
						*hi->iencptr++ = c ^ 0x40;
					else
					{
						log("Hydra: packet too long");
						hi->iencrcv = FALSE;
						hi->iencptr = hi->iencbuf;
					}
				}
#ifdef DEBUG
				else
					DEB((D_PROT, "hydra_recv: ignore 0x%02x (escaped)", c ^ 0x40));
#endif
			}
		}
		else if( hi->iencrcv )
		{
			if( hi->iencptr - hi->iencbuf < HYDRA_BUFLEN )
				*hi->iencptr++ = c;
			else
			{
				log("Hydra: packet too long");
				hi->iencrcv = FALSE;
				hi->iencptr = hi->iencbuf;
			}
		}
#ifdef DEBUG
		else
			DEB((D_PROT, "hydra_recv: ignore 0x%02x", c));
#endif
	}
	
	return (c == TTY_TIMEOUT) ? HPKT_NONE : HPKT_EXIT;
}

/*
 * Parse received INIT packet. Return -1 value if got unparsable/invalid
 * INIT packet. Probably we should abort transfer in case of such error.
 */
static int hydra_parse_init(s_hydrainfo *hi, char *pkt, size_t pktlen)
{
	char *appinf;	/* Application info */
	char *canopt;	/* Supported options */
	char *desopt;	/* Desired options */
	char *window;	/* Tx/Rx window sizes */
	char *prefix;	/* Prefix string */
	char *endptr = pkt + pktlen;
	time_t revtime;
	
	if( pktlen > 8 && pkt[pktlen-1] == '\0' )
	{
		/*
		 * Get other's Hydra revision time stamp
		 */
		sscanf(pkt, "%08lx", (unsigned long *)&revtime);
		
		/*
		 * Get other's application info
		 */
		appinf = pkt + 8;
		
		/*
		 * Get other's supported options, desired options,
		 * rx/tx windows sizes and packet prefix string
		 */
		canopt = (appinf && appinf < endptr) ? appinf + strlen(appinf) + 1 : NULL;
		desopt = (canopt && canopt < endptr) ? canopt + strlen(canopt) + 1 : NULL;
		window = (desopt && desopt < endptr) ? desopt + strlen(desopt) + 1 : NULL;
		prefix = (window && window < endptr) ? window + strlen(window) + 1 : NULL;

		if( appinf && canopt && desopt && window && prefix )
		{
			char buf[256];
			long txwindow = 0L;
			long rxwindow = 0L;
	
			DEB((D_PROT, "hydra: revtime = \"%s\"", time_string_long(buf, sizeof(buf), revtime)));
			DEB((D_PROT, "hydra: appinf  = \"%s\"", appinf));
			DEB((D_PROT, "hydra: canopt  = \"%s\"", canopt));
			DEB((D_PROT, "hydra: desopt  = \"%s\"", desopt));
			DEB((D_PROT, "hydra: window  = \"%s\"", window));
			DEB((D_PROT, "hydra: prefix  = \"%s\"", prefix));
	
			hi->rxoptions  = hi->options;
			hi->rxoptions |= hydra_getflags(desopt);
			hi->rxoptions &= hydra_getflags(canopt);
			hi->rxoptions &= HYDRA_CANOPT;
			hi->txoptions  = hi->rxoptions;
	
			if( hi->rxoptions < (hi->options & HYDRA_NECOPT) )
			{
				log("Hydra is incompartible on this link");
				return -1;
			}
						
			sscanf(window, "%08lx%08lx", &txwindow, &rxwindow);
						
			if( txwindow < 0 ) txwindow = 0L;
			if( rxwindow < 0 ) rxwindow = 0L;
			
			/*
			 * Log other's hydra information only at 1st batch
			 */
			if( !hi->batchesdone )
			{
				if( hi->rxoptions )
				{
					hydra_putflags(buf, hi->rxoptions, sizeof(buf));
					log("Hydra init \"%s\", \"%s\"",
						string_printable(appinf), buf);
				}
				else
				{
					log("Hydra init \"%s\"",
						string_printable(appinf));
				}
				hi->batchesdone = TRUE;
			}
			
			if( (hi->txwindow != txwindow)
			 || (hi->rxwindow != rxwindow) )
			{
				log("Hydra Tx/Rx windows = %ld/%ld bytes",
					txwindow, rxwindow);
				hi->txwindow = txwindow;
				hi->rxwindow = rxwindow;
			}
			
			return 0;
		}
	}
	
	log("Hydra got invalid INIT packet");
	
	return -1;
}

static void hydra_align_block_size(int *block_size)
{
	int n = *block_size;

	if( n <= 64 )
		n = 64;
	else if( n <= 128 )
		n = 128;
	else if( n <= 256 )
		n = 256;
	else if( n <= 512 )
		n = 512;
	else if( n <= 1024 )
		n = 1024;
	else
		n = 2048;
	
	*block_size = n;
}

static void hydra_init(s_hydrainfo *hi)
{
	memset(hi, '\0', sizeof(s_hydrainfo));
	
	/*
	 * RPOS ID sequence starting value
	 */
	hi->recv_rpos_id = 100;
	hi->send_rpos_id = -1;
	
	/*
	 * Set rx/tx block sizes
	 */
	hi->send_block_size = 1024;
	hi->recv_block_size = 1024;

	/*
	 * Set timeout values
	 */
	hi->timeout_send = 30;
	hi->timeout_recv = 30;
	hi->timeout_dead = 120;
	
	/*
	 * Set start up link options
	 */
	hi->options   = HYDRA_DEFOPT & HYDRA_CANOPT;
	hi->txoptions = HYDRA_TXIOPT;
	hi->rxoptions = HYDRA_RXIOPT;
	
	hi->lastchar = -1;
	hi->oencbuf  = xmalloc(HYDRA_BUFLEN);
	hi->oencptr  = hi->oencbuf;
	hi->iencbuf  = xmalloc(HYDRA_BUFLEN);
	hi->iencptr  = hi->iencbuf;
	hi->obuf     = xmalloc(HYDRA_MAXDATALEN);
	hi->ibuf     = xmalloc(HYDRA_MAXDATALEN);
}

static void hydra_deinit(s_hydrainfo *hi)
{
	if( hi->iencbuf ) free(hi->iencbuf);
	if( hi->oencbuf ) free(hi->oencbuf);
	if( hi->ibuf    ) free(hi->ibuf);
	if( hi->obuf    ) free(hi->obuf);
	
	memset(hi, '\0', sizeof(s_hydrainfo));
}

int hydra_batch(s_hydrainfo *hi, s_protinfo *pi)
{
	int rc = PRC_NOERROR;
	int n;
	char *p;
	bool send_EOB = FALSE;
	int send_rc = 0;
	int recv_rc = 0;
	bool send_ready = FALSE;
	bool recv_ready = FALSE;
	enum hydra_txstate txstate = HTX_START;
	enum hydra_rxstate rxstate = HRX_INIT;
	int txtries = 0;
	int rxtries = 0;
	long txlastack = 0L;
	long rxlastack = 0L;
	time_t idletimer = 0;
	time_t sendtimer = 0;
	time_t recvtimer = 0;
	time_t deadtimer = 0;
#ifdef DEBUG
	int prev_txstate = -1;
	int prev_rxstate = -1;
#endif
	long last_datapos = -1;   /* The last received DATA block offset */
	
	timer_set(&deadtimer, hi->timeout_dead);
	
	while( txstate != HTX_DONE || rxstate != HRX_DONE )
	{
#ifdef DEBUG
		if( txstate != prev_txstate || rxstate != prev_rxstate )
		{
			prev_txstate = txstate;
			prev_rxstate = rxstate;
			DEB((D_PROT, "hydra: txstate = '%s' (%d), rxstate = '%s' (%d)",
				HYDRA_TXSTATE(txstate), txstate,
				HYDRA_RXSTATE(rxstate), rxstate));
		}
#endif
		
		if( timer_running(deadtimer) && timer_expired(deadtimer) )
		{
			log("remote dead (timer expired)");
			gotoexit(PRC_ERROR);
		}
		
		if( timer_running(sendtimer) && timer_expired(sendtimer) )
		{
			sendtimer = 0;
			switch(txstate) {
			case HTX_SWAIT:    txstate = HTX_START; break;
			case HTX_INITACK:  txstate = HTX_INIT;  break;
			case HTX_FINFOACK: txstate = HTX_FINFO; break;
			case HTX_DATAACK:  txstate = HTX_DATA;  break; /* Incorrect! */
			case HTX_EOFACK:   txstate = HTX_EOF;   break;
			case HTX_ENDACK:   txstate = HTX_END;   break;
			default:
				log("sendtimer expired, but txstate = %s #%d",
					HYDRA_TXSTATE(txstate), txstate);
			}
			DEB((D_PROT, "hydra: send timer expired, new txstate = '%s' #%d",
				HYDRA_TXSTATE(txstate), txstate));
		}
		
		if( timer_running(recvtimer) && timer_expired(recvtimer) )
		{
			recvtimer = 0;
			switch(rxstate) {
			case HRX_RPosAck:
				/* Decrease data block size */
				hi->recv_block_size >>= 1;
				hydra_align_block_size(&hi->recv_block_size);
				
				/* Send RPOS packet again */
				rxstate = HRX_RPOS;
				break;
				
			case HRX_SkipAck: rxstate = HRX_Skip; break;
			case HRX_DATA:    rxstate = HRX_RPOS; break;
			default:
				log("recvtimer expired, but rxstate = %s #%d",
					HYDRA_RXSTATE(rxstate), rxstate);
			}
			DEB((D_PROT, "hydra: recv timer expired, new rxstate = '%s' #%d",
				HYDRA_RXSTATE(rxstate), rxstate));
		}
		
		switch(txstate) {
		case HTX_START:
			if( ++txtries < 10 )
			{
				timer_set(&sendtimer, hi->timeout_send);
				PUTSTR("hydra\r");
				hydra_queuepkt(hi, NULL, 0, HPKT_START);
				txstate = HTX_SWAIT;
			}
			else
			{
				log("Hydra unable to initiate session");
				gotoexit(PRC_ERROR);
			}
			break;
			
		case HTX_INIT:
			if( ++txtries < 10 )
			{
				timer_set(&sendtimer, hi->timeout_send);
				
				p = hi->obuf;
				
				/* Application info */
				p += sprintf(p, "%08lx%s,%s", (long)0L, BF_NAME, BF_VERSION) + 1;
				
				/* What we can */
				p = hydra_putflags(p, HYDRA_CANOPT, 256) + 1;
				
				/* What we want */
				p = hydra_putflags(p, HYDRA_DEFOPT, 256) + 1;
				
				/* Tx/Rx window sizes */
				p += sprintf(p, "%08lx%08lx", (long)0L, (long)0L) + 1;
				
				/* Pkt prefix string we want */
				*p++ = '\0';
				
				hydra_queuepkt(hi, hi->obuf, p - hi->obuf, HPKT_INIT);
				txstate = HTX_INITACK;
			}
			else
			{
				log("Hydra too many tries sending INIT packet");
				gotoexit(PRC_ERROR);
			}
			break;

		case HTX_RINIT:
			if( rxstate != HRX_INIT )
				txstate = HTX_NextFile;
			break;
		
		case HTX_NextFile:
			if( pi->send && pi->send->fp )
				p_tx_fclose(pi);
			
			send_EOB = p_tx_fopen(pi) ? TRUE : FALSE;
			
			txtries = 0;
			txstate = HTX_FINFO;
			
			/* FALL THROUGH */
			
		case HTX_FINFO:
			if( ++txtries < 10 )
			{
				timer_set(&sendtimer, hi->timeout_send);
				
				if( send_EOB == FALSE )
				{
					char dosname[13];
					
					n = sprintf(hi->obuf, "%08lx%08lx%08lx%08lx%08lx",
							(long)pi->send->mod_time,
							(long)pi->send->bytes_total,
							(long)0L, (long)0L, (long)0L);
					
					file_get_dos_name(dosname, pi->send->net_name);
					strnxcpy(hi->obuf + n, dosname, BFORCE_MAX_NAME);
					n += strlen(hi->obuf + n) + 1;
					
					if( strcmp(dosname, pi->send->net_name) )
					{
						strnxcpy(hi->obuf + n, pi->send->net_name, BFORCE_MAX_NAME);
						n += strlen(hi->obuf + n) + 1;
					}
				}
				else /* Send 'EOB' */
				{
					n = 1;
					hi->obuf[0] = '\0';
				}
			
				hydra_queuepkt(hi, hi->obuf, n, HPKT_FINFO);
				txstate = HTX_FINFOACK;
			}
			else
			{
				log("Hydra too many tries sending FINFO packet");
				gotoexit(PRC_ERROR);
			}
			break;
		
		case HTX_DATA:
			if( pi->send->eofseen )
			{
				txtries = 0;
				txstate = HTX_EOF;
			}
			else if( hi->oencpos == 0 && hi->n_pkts == 0 )
			{
				sendtimer = 0;				
				
				n = p_tx_readfile(hi->obuf+4, hi->send_block_size, pi);
				if( n > 0 )
				{
					timer_set(&deadtimer, hi->timeout_dead);
					
					hydra_putlong(hi->obuf, (long)pi->send->bytes_sent);
					pi->send->bytes_sent += n;
					
					p = hydra_putpkt(hi, hi->oencbuf, hi->obuf, n+4, HPKT_DATA);
					hi->oencpos = p - hi->oencbuf;
					
					/* Increase number of good blocks */
					++hi->send_good_blocks;
					
					/* Is it a good time for raising block size? */
					if( hi->send_block_size < 2048 )
					{
						if( (hi->send_good_blocks * hi->send_block_size) >= 4096 )
						{
							hi->send_good_blocks = 0;
							hi->send_block_size <<= 1;
							hydra_align_block_size(&hi->send_block_size);
							DEB((D_PROT, "hydra: Tx now use %ld bytes blocks",
									(long)hi->send_block_size));
						}
					}
					
					if( (hi->txwindow > 0)
					 && (pi->send->bytes_sent >= txlastack + hi->txwindow) )
					{
						/*
						 * Tx window reached its maximal size,
						 * continue sending only after receiving
						 * of DATAACK packet with correct offset
						 */
						if( txtries > 0 )
							timer_set(&sendtimer, hi->timeout_send/2);
						else
							timer_set(&sendtimer, hi->timeout_send);
	
						txstate = HTX_DATAACK;
					}
					else if( pi->send->eofseen )
					{
						txtries = 0;
						txstate = HTX_EOF;
					}
				}
				else
				{
					txtries = 0;
					txstate = HTX_EOF;
				}
			}
			break;
		
		case HTX_EOF:
			if( ++txtries < 10 )
			{
				timer_set(&sendtimer, hi->timeout_send);
				
				if( pi->send )
				{
					if( pi->send->status == FSTAT_SKIPPED )
						hydra_putlong(hi->obuf, (long)-1L);
					else if( pi->send->status == FSTAT_REFUSED )
						hydra_putlong(hi->obuf, (long)-2L);
					else if( pi->send->eofseen )
						hydra_putlong(hi->obuf, (long)pi->send->bytes_sent);
					else
						hydra_putlong(hi->obuf, (long)-2L);
				}
				else
					hydra_putlong(hi->obuf, (long)-2L);
				
				hydra_queuepkt(hi, hi->obuf, 4, HPKT_EOF);
				txstate = HTX_EOFACK;
			}
			else
			{
				log("Hydra too many tries sending EOF packet");
				gotoexit(PRC_ERROR);
			}
			break;

		case HTX_REND:
			if( rxstate == HRX_DONE )
			{
				txtries = 0;
				txstate = HTX_END;
			}
			else if( !timer_running(idletimer) || timer_expired(idletimer) )
			{
				timer_set(&idletimer, 20);
				hydra_queuepkt(hi, NULL, 0, HPKT_IDLE);
			}
			break;
			
		case HTX_END:
			if( ++txtries < 10 )
			{
				timer_set(&sendtimer, hi->timeout_send);
				hydra_queuepkt(hi, NULL, 0, HPKT_END);
				txstate = HTX_ENDACK;
			}
			else
			{
				log("Hydra too many tries sending END packet");
				gotoexit(PRC_ERROR);
			}
			break;
			
		default:
			/* Do nothing, but avoid warning message */
			break;
		}
		
		switch(rxstate) {
		case HRX_RPOS:
		case HRX_Skip:
			if( ++rxtries < 10 )
			{
				long pos;
				
				timer_set(&recvtimer, hi->timeout_recv);

				if( rxstate == HRX_Skip )
					pos = (pi->recv->status == FSTAT_SKIPPED) ? -1L : -2L;
				else
					pos = pi->recv->bytes_received;
				
				hydra_putlong(hi->obuf, pos);
				hydra_putword(hi->obuf + 4, hi->recv_block_size);
				hydra_putlong(hi->obuf + 6, hi->recv_rpos_id);
				hydra_queuepkt(hi, hi->obuf, 10, HPKT_RPOS);
				
				rxstate = (rxstate == HRX_Skip) ? HRX_SkipAck : HRX_RPosAck;
			}
			else
			{
				log("Hydra too many tries sending RPOS packet");
				gotoexit(PRC_ERROR);
			}
			break;
			
		default:
			/* Do nothing, but avoid warning message */
			break;
		}

#ifdef DEBUG
		if( txstate != prev_txstate || rxstate != prev_rxstate )
		{
			prev_txstate = txstate;
			prev_rxstate = rxstate;
			DEB((D_PROT, "hydra: txstate = '%s' (%d), rxstate = '%s' (%d)",
				HYDRA_TXSTATE(txstate), txstate,
				HYDRA_RXSTATE(rxstate), rxstate));
		}
#endif
		
		/*
		 * Check current CPS, session time limits, etc.
		 */
		if( (rc = p_info(pi, 1)) ) gotoexit(rc);
		
		/*
		 * Send/receive as much data as possible, but without delays
		 */
		send_ready = recv_ready = FALSE;

		if( hi->oencpos > 0 || hi->n_pkts )
			n = tty_xselect(&recv_ready, &send_ready, 10);
		else
			n = tty_xselect(&recv_ready, NULL, 10);
		
		if( n < 0 && n != TTY_TIMEOUT )
			gotoexit(PRC_ERROR);
		
		recv_rc = HPKT_NONE;
		send_rc = 0;
		
		if( recv_ready && (recv_rc = hydra_recv(hi)) == HPKT_EXIT )
			gotoexit(PRC_ERROR);
		if( send_ready && (send_rc = hydra_send(hi)) < 0 )
			gotoexit(PRC_ERROR);
		
		switch(recv_rc) {
		case HPKT_NONE:
			break;

		case HPKT_JUNK:
			if( rxstate == HRX_DATA )
			{
				/* Increase RPOS packet ID number */
				hi->recv_rpos_id++;
				
				/* Decrease data block size */
				hi->recv_block_size >>= 1;
				hydra_align_block_size(&hi->recv_block_size);
				
				rxtries = 0;
				rxstate = HRX_RPOS;
			}
			break;
			
		case HPKT_START:
			if( txstate == HTX_START || txstate == HTX_SWAIT )
			{
				txtries = 0;
				txstate = HTX_INIT;
			}
			break;
			
		case HPKT_INIT:
			if( rxstate == HRX_INIT )
			{
				if( hydra_parse_init(hi, hi->ibuf, hi->isize) )
				{
					gotoexit(PRC_ERROR);
				}
				rxtries = 0;
				rxstate = HRX_FINFO;
			}
			hydra_queuepkt(hi, NULL, 0, HPKT_INITACK);
			break;
			
		case HPKT_INITACK:
			if( txstate == HTX_INIT || txstate == HTX_INITACK )
			{
				txtries = 0;
				txstate = HTX_RINIT;
			}
			break;
			
		case HPKT_FINFO:
			if( rxstate == HRX_FINFO )
			{
				/* Check for EOB FINFO packet */
				if( hi->ibuf[0] == '\0' )
				{
					hydra_putlong(hi->obuf, 0L);
					hydra_queuepkt(hi, hi->obuf, 4, HPKT_FINFOACK);
					rxstate = HRX_DONE;
				}
				else if( hi->isize > 41 && hi->ibuf[hi->isize-1] == '\0' )
				{
					time_t modtime;
					size_t filesize;
					char  *filename;
					char  *p;
					
					/* Get file modification time and size */
					sscanf(hi->ibuf, "%08lx%08x%*08x%*08x%*08x",
						(unsigned long *)&modtime, &filesize);
					
					/* Convert local time -> UTC */
					modtime = localtogmt(modtime);
					
					/* Select short file name */
					filename = p = hi->ibuf + 40;
					while( *p && p < (hi->ibuf + hi->isize) )
						++p;
					
					if( p == filename )
					{
						/* Got FINFO without file name */
						filename = "bad_name";
					}
					else if( *p )
					{
						/* Got not null-terminated file name */
						*(++p) = '\0';
					}
					else
					{
						/* Try long file name */
						char *long_filename = ++p;
						
						while( *p && p < (hi->ibuf + hi->isize) )
							++p;
						
						/* Accept only null-terminated long file names */
						if( *p == '\0' && p > long_filename )
							filename = long_filename;
					}
					
					switch( p_rx_fopen(pi, filename, filesize, modtime, 0) ) {
					case 0: /* No errors */
						rxlastack = (long)pi->recv->bytes_skipped;
						last_datapos = -1;
						hydra_putlong(hi->obuf, (long)pi->recv->bytes_skipped);
						timer_set(&recvtimer, hi->timeout_recv);
						rxstate = HRX_DATA;
						break;
						
					case 1:	/* SKIP (non-destructive) */
						hydra_putlong(hi->obuf, -2L);
						break;
						
					case 2: /* SKIP (destructive) */
						hydra_putlong(hi->obuf, -1L);
						break;
						
					default:
						ASSERT(0);
					}
					hydra_queuepkt(hi, hi->obuf, 4, HPKT_FINFOACK);
				}
				else
					log("Hydra: got invalid FINFO packet (ignored)");
			}
			else if( rxstate == HRX_DONE )
			{
				hydra_putlong(hi->obuf, -2);
				hydra_queuepkt(hi, hi->obuf, 4, HPKT_FINFOACK);
			}
			else
				log("Hydra: unexpected FINFO packet (ignored)");
			break;
			
		case HPKT_FINFOACK:
			if( txstate == HTX_FINFO || txstate == HTX_FINFOACK )
			{
				long offs = 0L;
				
				if( send_EOB == TRUE )
				{
					timer_set(&idletimer, 20);
					sendtimer = 0;
					txtries = 0;
					txstate = HTX_REND;
				}
				else if( hi->isize < 4 )
				{
					log("Hydra: got invalid FINFOACK packet (ignored)");
				}
				else if( (offs = hydra_getlong(hi->ibuf)) == 0 )
				{
					txlastack = 0;
					txtries = 0;
					txstate = HTX_DATA;
				}
				else if( offs > 0 )
				{
					if( fseek(pi->send->fp, offs, SEEK_SET) == 0 )
					{
						txlastack = offs;
						pi->send->bytes_skipped = pi->send->bytes_sent = offs;
						log("send file from offset %ld bytes", offs);
						txtries = 0;
						txstate = HTX_DATA;
					}
					else
					{
						logerr("can't send file from requested offset %ld", offs);
						p_tx_fclose(pi);
						txstate = HTX_NextFile;
					}
				}
				else if( offs == -1 )
				{
					pi->send->status = FSTAT_SKIPPED;
					log("remote allready has file");
					p_tx_fclose(pi);
					txstate = HTX_NextFile;
				}
				else /* Send file later */
				{
					pi->send->status = FSTAT_REFUSED;
					log("remote refused file");
					p_tx_fclose(pi);
					txstate = HTX_NextFile;
				}
			}
			break;
			
		case HPKT_DATA:
			if( rxstate == HRX_DATA || rxstate == HRX_RPosAck )
			{
				if( hi->isize < 4 )
				{
					log("Hydra: got invalid DATA packet (ignored)");
				}
				else if( hydra_getlong(hi->ibuf) != pi->recv->bytes_received )
				{
					long pos = hydra_getlong(hi->ibuf);
					
					if( rxstate == HRX_RPosAck )
					{
						if( pos < last_datapos )
							rxstate = HRX_RPOS;
					}
					else /* rxstate == HRX_DATA */
					{
						/* Increase RPOS packet ID number */
						hi->recv_rpos_id++;
						
						/* Decrease data block size */
						hi->recv_block_size >>= 1;
						hydra_align_block_size(&hi->recv_block_size);
						
						rxtries = 0;
						rxstate = HRX_RPOS;
					}
					last_datapos = pos;
				}
				else /* Write to the file */
				{
					hi->recv_block_size = hi->isize-4;
					last_datapos = hydra_getlong(hi->ibuf);

					timer_set(&deadtimer, hi->timeout_dead);
					timer_set(&recvtimer, hi->timeout_recv);
					
					if( rxstate == HRX_RPosAck )
						rxstate = HRX_DATA;
					
					switch(p_rx_writefile(hi->ibuf+4, hi->isize-4, pi)) {
					case  0:
						if( hi->rxwindow > 0 )
						{
							/*
							 * It is not the best idea to
							 * acknowledge every data packet,
							 * but is so easy to implement :)
							 */
							rxlastack = (long)pi->recv->bytes_received;
							hydra_putlong(hi->obuf, (long)pi->recv->bytes_received);
							hydra_queuepkt(hi, hi->obuf, 4, HPKT_DATAACK);
						}
						
						pi->recv->bytes_received += hi->isize-4;
						break;
						
					case -1: /* SKIP (non-destructive) */
						pi->recv->status = FSTAT_REFUSED;
						hi->recv_rpos_id++;
						rxtries = 0;
						rxstate = HRX_Skip;
						break;
						
					case -2: /* SKIP (destructive) */
						pi->recv->status = FSTAT_SKIPPED;
						hi->recv_rpos_id++;
						rxtries = 0;
						rxstate = HRX_Skip;
						break;
						
					default:
						ASSERT(0);
					}
				}
			}
			break;
			
		case HPKT_DATAACK:
			if( txstate == HTX_DATA || txstate == HTX_DATAACK )
			{
				long ackpos = hydra_getlong(hi->ibuf);
				
				if( ackpos >= pi->send->bytes_sent )
					log("Hydra got DATAACK from feature");
				else if( ackpos < txlastack )
					log("Hydra got DATAACK from past");
				else
				{
					txlastack = ackpos;
					
					if( (txstate == HTX_DATAACK)
					 && (pi->send->bytes_sent < txlastack + hi->txwindow) )
					{
						txtries = 0;
						txstate = HTX_DATA;
					}
				}
			}
			break;
			
		case HPKT_RPOS:
			if( txstate == HTX_DATA || txstate == HTX_DATAACK
			 || txstate == HTX_XWAIT
			 || txstate == HTX_EOF || txstate == HTX_EOFACK )
			{
				if( hi->isize < 10 )
				{
					log("Hydra: got invalid RPOS packet (ignored)");
				}
				else
				{
					long offset    = hydra_getlong(hi->ibuf);
					int blocksize  = hydra_getword(hi->ibuf + 4);
					long packetId  = hydra_getlong(hi->ibuf + 6);
					
					/* Check requested block size */
					if( blocksize > 0 )
						hydra_align_block_size(&blocksize);
					
					if( packetId > 0 && packetId == hi->send_rpos_id )
					{
						log("Hydra Tx got duplicated RPOS packet (ignore)");
					}
					else if( offset >= 0 )
					{
						/* Reset EOF flag! */
						pi->send->eofseen = FALSE;
							
						/* Store ID of the processed RPOS */
						hi->send_rpos_id = packetId;
												
						if( fseek(pi->send->fp, offset, SEEK_SET) == 0
						 && ftell(pi->send->fp) == offset )
						{ 
							txlastack = offset; /* Is it OK? */

							log("Hydra Tx resend from %ld",
								(long)offset);
							
							pi->send->bytes_sent = offset;
							
							if( blocksize && blocksize != hi->send_block_size )
							{
								log("Hydra Tx use %ld bytes blocks",
										(long)blocksize);
								hi->send_block_size = blocksize;
								hi->send_good_blocks = 0;
							}
							
							if( txstate != HTX_XWAIT )
								txstate = HTX_DATA;
						}
						else
						{
							log("Hydra Tx can't resend from %ld", (long)offset);
							
							pi->send->status = FSTAT_REFUSED;
							txtries = 0;
							txstate = HTX_EOF;
						}
					}
					else if( offset == -1 )
					{
						log("Hydra Tx skipping file");
						
						/* Store ID of the processed RPOS */
						hi->send_rpos_id = packetId;
						
						pi->send->status = FSTAT_SKIPPED;
						txtries = 0;
						txstate = HTX_EOF;
					}
					else
					{
						log("Hydra Tx refusing file");
						
						/* Store ID of the processed RPOS */
						hi->send_rpos_id = packetId;

						pi->send->status = FSTAT_REFUSED;
						txtries = 0;
						txstate = HTX_EOF;
					}
				}
			}
			break;
			
		case HPKT_EOF:
			if( rxstate == HRX_DATA )
			{
				if( hi->isize < 4 )
				{
					log("Hydra: got invalid EOF packet (ignored)");
				}
				else if( hydra_getlong(hi->ibuf) != pi->recv->bytes_received )
				{
					last_datapos = hydra_getlong(hi->ibuf);
					
					/* Increase RPOS packet ID number */
					hi->recv_rpos_id++;
						
					/* Decrease data block size */
					hi->recv_block_size >>= 1;
					hydra_align_block_size(&hi->recv_block_size);
						
					rxtries = 0;
					rxstate = HRX_RPOS;
				}
				else
				{
					pi->recv->status = FSTAT_SUCCESS;
					
					if( p_rx_fclose(pi) == 0 )
					{
						hydra_queuepkt(hi, NULL, 0, HPKT_EOFACK);
						rxtries = 0;
						rxstate = HRX_FINFO;
					}
					else /* Error closing file */
					{
						pi->recv->status = FSTAT_REFUSED;
						rxtries = 0;
						rxstate = HRX_Skip;
					}
				}
			}
			else if( rxstate == HRX_Skip || rxstate == HRX_SkipAck )
			{
				if( hi->isize < 4 )
				{
					log("Hydra: got invalid EOF packet (ignored)");
				}
				else if( hydra_getlong(hi->ibuf) >= 0 )
				{
					/*
					 * Possible they didn't seen our
					 * RPOS(-2) packet. If we send EOFACK
					 * packet now, file will be lost! :(
					 * There is such "bug" in 'Argus'?
					 * So we will Skip it again, again..
					 */
					rxstate = HRX_Skip;
				}
				else
				{
					hydra_queuepkt(hi, NULL, 0, HPKT_EOFACK);
					(void)p_rx_fclose(pi);
					recvtimer = 0;
					rxtries = 0;
					rxstate = HRX_FINFO;
				}
			}
			else if( hi->isize == 4 && hydra_getlong(hi->ibuf) == -2 )
			{
				/*
				 * We will ack. all received EOF packets
				 * (even unexpected) with (-2) offset,
				 * it is not harmfull.
				 */
				hydra_queuepkt(hi, NULL, 0, HPKT_EOFACK);
			}
			else
			{
				log("Hydra got unexpected EOF packet");
			}
			break;
			
		case HPKT_EOFACK:
			if( txstate == HTX_EOF || txstate == HTX_EOFACK )
			{
				if( pi->send )
				{
					if( pi->send->eofseen )
						pi->send->status = FSTAT_SUCCESS;
					p_tx_fclose(pi);
					txstate = HTX_NextFile;
				}
				else
				{
					txtries = 0;
					txstate = HTX_END;
				}
			}
			break;
			
		case HPKT_END:
			if( txstate == HTX_END || txstate == HTX_ENDACK )
			{
				hydra_queuepkt(hi, NULL, 0, HPKT_END);
				hydra_queuepkt(hi, NULL, 0, HPKT_END);
				hydra_queuepkt(hi, NULL, 0, HPKT_END);
				txtries = 0;
				txstate = HTX_DONE;
			}
			break;
			
		case HPKT_IDLE:
			timer_set(&recvtimer, hi->timeout_recv);
			break;
			
		case HPKT_DEVDATA:
			log("Hydra: ignore DEVDATA packet");
			break;
			
		case HPKT_DEVDACK:
			log("Hydra: ignore DEVDACK packet");
			break;
			
		default:
			log("Hydra: unhandled packet type #%d", recv_rc);
		}
	}
	
	/*
	 * Send queued packets and buffered data
	 */
	txtries = 0;
	while( hi->oencpos > 0 || hi->n_pkts > 0 )
	{
		if( ++txtries > 20 )
			gotoexit(PRC_ERROR);
		
		if( tty_select(NULL, &send_ready, 30) < 0 )
			gotoexit(PRC_ERROR);
		
		if( !send_ready || hydra_send(hi) )
			gotoexit(PRC_ERROR);
	}
	FLUSHOUT();
	
exit:
	if( pi->send && pi->send->fp )
		p_tx_fclose(pi);
	if( pi->recv && pi->recv->fp )
		p_rx_fclose(pi);
	
	return rc;
}

int hydra(s_protinfo *pi, bool files_after_freqs)
{
	int rc;
	s_hydrainfo hi;

	log("start Hydra send+receive%s",
		files_after_freqs ? " (RH1 mode)" : "");
	
	hydra_init(&hi);
	
	/*
	 * Send only file requests during first
	 * batch if EMSI 'RH1' flag specified
	 */
	if( files_after_freqs )
		pi->reqs_only = TRUE;
	
	rc = hydra_batch(&hi, pi);
	
	if( rc == PRC_NOERROR )
	{
		if( files_after_freqs )
			pi->reqs_only = FALSE;
		
		rc = hydra_batch(&hi, pi);
	}
	
	hydra_deinit(&hi);
	
	return rc;
}

