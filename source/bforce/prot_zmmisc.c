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
#include "io.h"
#include "prot_zmodem.h"

int  Z_Rxexp;		/* True means timer expired                  */
int  Z_Rxtout;		/* Seconds to wait for receiving whole block */
int  Z_Rxwait;		/* Seconds to wait for character available   */
int  Z_Rxframeind;	/* ZBIN ZBIN32, or ZHEX type of frame received */
int  Z_Rxtype;		/* Type of header received                   */
int  Z_Rxcount;		/* Count of data bytes received              */
int  Z_Txnulls;		/* Number of nulls to send at beginning of ZDATA hdr */
char Z_Rxhdr[4];	/* Received header                           */
char Z_Txhdr[4];	/* Transmitted header                        */
long Z_Rxpos;		/* Received file position                    */
int  Z_Txfcs32;		/* TRUE means send binary frames with 32 bit FCS */
int  Z_Txcrc32;		/* Display flag indicating 32 bit CRC being sent */
int  Z_Rxcrc32;		/* Display flag indicating 32 bit CRC being received */
char Z_Attn[ZATTNLEN+1];/* Attention string rx sends to tx on err     */
int  Z_Ctlesc;		/* Encode control characters                 */
int  Z_Lastsent;	/* Character we sent last                    */

#ifdef DEBUG
static const char *FrameTypes[] =
{
	"Unused",		/*           (-7) */
	"Unused",		/*           (-6) */
	"CRC error",		/* ZCRCERR   (-5) */
	"Bad frame",		/* ZERROR    (-4) */
	"I/O Error",		/* ZEXIT     (-3) */
	"Carrier Lost",		/* ZHANGUP   (-2) */
	"Timer out",		/* ZTIMER    (-1) */
#define FTOFFSET 7
	"ZRQINIT",
	"ZRINIT",
	"ZSINIT",
	"ZACK",
	"ZFILE",
	"ZSKIP",
	"ZNAK",
	"ZABORT",
	"ZFIN",
	"ZRPOS",
	"ZDATA",
	"ZEOF",
	"ZFERR",
	"ZCRC",
	"ZCHALLENGE",
	"ZCOMPL",
	"ZCAN",
	"ZFREECNT",
	"ZCOMMAND",
	"ZSTDERR",
	"Unused"
#define FRTYPES 22		/* Total number of frame types in this array */
				/*  not including psuedo negative entries    */
};
#endif

static int  zrbhdr16(char *hdr);
static int  zrbhdr32(char *hdr);
static int  zrhhdr(char *hdr);
static int  zrdat32(char *buf, int length, long pos);
static int  zrdat16(char *buf, int length, long pos);
static int  zgethex(void);
static int  noxrd7(void);
static int  zdlread(void);
static int  zputhex(int c);
static void zsendline_init(char *);

static void zalarmh(int sig_no)
{
	DEB((D_PROT, "zalarmh: got SIGALRM (timer expired!)"));
	Z_Rxexp = 1;
}

void setalarm(int sec)
{
	static int allready = 0;
	
	if( !sec )
	{
		alarm(0); return;
	}
	
	Z_Rxexp = 0;
	
	if( allready == 0 )
	{
		if( signal(SIGALRM, zalarmh) == SIG_ERR )
		{
			logerr("can't setup SIGALRM handler");
			return;
		}
		allready = 1;
	}
	
	alarm(sec);
}

/* ------------------------------------------------------------------------- */
/* Read a byte, checking for ZMODEM escape encoding                          */
/* including five CAN which represents a quick abort                         */
/* ------------------------------------------------------------------------- */
static int zmodem_getbyte(void)
{
	int c;
	int cancount = 0;
	int gotdle = 0;
	
	while(1)
	{
		if( Z_Rxexp )
			return ZTIMER;

		if( (c = GETCHAR(Z_Rxwait)) < 0 )
			return c;
		
		if( gotdle )
		{
			if( c != CAN )
				cancount = 0;

			switch( c ) {
			case CAN:
				if( ++cancount > 5 )
					return GOTCAN;
				break;
			case ZCRCE:
			case ZCRCG:
			case ZCRCQ:
			case ZCRCW:
				return c|GOTOR;
			case ZRUB0:
				return 0177;
			case ZRUB1:
				return 0377;
			case XON:
			case XOFF:
			case XON|0200:
			case XOFF|0200:
				break;
			default:
				if( !Z_Ctlesc || (c & 140) )
				{
					if( (c & 0140) == 0100 )
						return c^0100;
					else
						log("zmodem: got bad escape sequence 0x%x", c);
				}
			}
		}
		else
		{
			switch(c) {
			case ZDLE:
				gotdle = 1;
				break;
			case XON:
			case XOFF:
			case XON|0200:
			case XOFF|0200:
				break;
			default:
				if( !Z_Ctlesc || (c & 0140) )
				{
					return c;
				}
			}
		}
	}

	return 0;
}

static int zmodem_getbyte_raw(void)
{
	int c;
	
	while(1)
	{
		if( Z_Rxexp )
			return ZTIMER;

		if( (c = GETCHAR(Z_Rxwait)) < 0 )
			return c;
		
		c &= 0177;
		
		switch(c) {
		case XON:
		case XOFF:
			break;
		case '\r':
		case '\n':
		case ZDLE:
			return c;
		default:
			if( !Z_Ctlesc || (c & 0140) )
				return c;
		}
	}

	return 0;
}

static int zmodem_putbyte(int c)
{
	static int last_esc = -2;
	static char tab[256];
	int rc;
	
	if( Z_Ctlesc != last_esc )
	{
		zsendline_init(tab);
		last_esc = Z_Ctlesc;
	}

	switch( tab[(unsigned char)(c)] ) {
	case 0: 
		if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
			return rc; 
		break;
		
	case 1:
		if( (rc = BUFCHAR(ZDLE)) < 0 )
			return rc;
		c ^= 0100;
		if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
			return rc;
		break;
		
	case 2:
		/* Escape for telnet escape sequence */
		if( (Z_Lastsent & 0177) != '@' )
		{
			if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
				return rc;
		}
		else
		{
			c ^= 0100;
			if( (rc = BUFCHAR(ZDLE)) < 0 )
				return rc;
			if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
				return rc;
		}
		break;

	default:
		ASSERT_MSG();
	}
	
	return ZOK;
}

static int zdlread(void)
{
	int c, cancnt = 0;
	
	/* Quick check for non control characters */
	for(;;)
	{
		if( Z_Rxexp )
			return(ZTIMER);
		
		c = GETCHAR(Z_Rxwait);
		
		if( (c & 0140) || (c < 0) )
			return(c);

		switch( c ) {
		case ZDLE:
			break;
		case XON:
		case XOFF:
		case XON|0200:
		case XOFF|0200:
			continue;
		default:
			if( Z_Ctlesc && !(c & 0140) )
				continue;
			return(c);
		}
		break;
	}

	for(;;)
	{
		if( Z_Rxexp )
			return(ZTIMER);
		
		if( (c = GETCHAR(Z_Rxwait)) < 0 )
			return(c);
		
		if( c != CAN )
			cancnt = 0;
		
		switch( c ) {
		case CAN:
			if( ++cancnt >= 5 )
				return(GOTCAN);
			continue;
		case ZCRCE:
		case ZCRCG:
		case ZCRCQ:
		case ZCRCW:
			return(c | GOTOR);
		case ZRUB0:
			return(0177);
		case ZRUB1:
			return(0377);
		case XON:
		case XOFF:
		case XON|0200:
		case XOFF|0200:
			continue;
		default:
			if( Z_Ctlesc && !(c & 0140) )
				continue;
			if( (c & 0140) == 0100 )
				return(c^0100);
			
			/* It was unknown escape sequence */
			DEB((D_PROT, "zdlread: bad escape sequence 0x%x", c));
			
			return(ZERROR);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Read a character from the modem line with timeout.                        */
/* Eat parity, XON and XOFF characters.                                      */
/* ------------------------------------------------------------------------- */
static int noxrd7(void)
{
	int c;

	for(;;)
	{
		if( Z_Rxexp )
			return(ZTIMER);
		
		if( (c = GETCHAR(Z_Rxwait)) < 0 )
			return(c);
		
		switch( c &= 0177 ) {
		case XON:
		case XOFF:
			continue;
		default:
			if( Z_Ctlesc && !(c & 0140) )
				continue;
		case '\r':
		case '\n':
		case ZDLE:
			return(c);
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Send character $c with ZMODEM escape sequence encoding.                   */
/* Escape XON, XOFF. Escape CR following @ (Telenet net escape)              */
/* Warning: Put result to buffer with BUFCHAR function! Use FLUSHBUF later!  */
/* ------------------------------------------------------------------------- */
int zsendline(char c)
{
	static int last_esc = -2;
	static char tab[256];
	int rc;
	
	if( Z_Ctlesc != last_esc )
	{
		zsendline_init(tab);
		last_esc = Z_Ctlesc;
	}

	switch( tab[(unsigned char)(c)] ) {
	case 0: 
		if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
			return(rc); 
		break;
	case 1:
		if( (rc = BUFCHAR(ZDLE)) < 0 )
			return(rc);
		c ^= 0100;
		if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
			return(rc);
		break;
	case 2:
		/* Escape for telnet escape sequence */
		if( (Z_Lastsent & 0177) != '@' )
		{
			if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
				return(rc);
		}
		else
		{
			c ^= 0100;
			if( (rc = BUFCHAR(ZDLE)) < 0 )
				return(rc);
			if( (rc = BUFCHAR(Z_Lastsent = c)) < 0 )
				return(rc);
		}
		break;
	}
	
	return(ZOK);
}

/* ------------------------------------------------------------------------- */
/* Send ZMODEM binary header hdr of type $type                               */
/* ------------------------------------------------------------------------- */
int zsbhdr(int type, char *hdr)
{
	int n, rc;
	unsigned short crc;
	unsigned long crc32;

	DEB((D_PROT, "zsbhdr: %s %lx (CRC%s)", FrameTypes[type+FTOFFSET],
		rclhdr(hdr), Z_Txfcs32?"32":"16"));
	    
	if( type == ZDATA )
		for( n=Z_Txnulls; --n >= 0; )
		{
			if( (rc = BUFCHAR('\0')) < 0 )
				return(rc);
		}

	if( (rc = BUFCHAR(ZPAD)) < 0 )
		return(rc);
	if( (rc = BUFCHAR(ZDLE)) < 0 )
		return(rc);

	Z_Txcrc32 = Z_Txfcs32;
	
	if( Z_Txcrc32 )
	{
		/* Use CRC32 */
		crc32 = 0xFFFFFFFFL;
		
		if( (rc = BUFCHAR(ZBIN32)) < 0 )
			return(rc);
		if( (rc = zsendline(type)) < 0 )
			return(rc);
		crc32 = updcrc32(type, crc32);

		for( n = 4; --n >= 0; ++hdr )
		{
			crc32 = updcrc32((0377 & *hdr), crc32);
			if( (rc = zsendline((*hdr) & 0377)) < 0 )
				return(rc);
		}
		
		crc32 = ~crc32;
		
		for( n = 4; --n >= 0; )
		{
			if( (rc = zsendline(crc32 & 0377)) < 0 )
				return(rc);
			crc32 >>= 8;
		}
	}
	else /* CRC-16 */
	{
		crc = 0;
		
		if( (rc = BUFCHAR(ZBIN)) < 0 )
			return rc;
		if( (rc = zsendline(type)) < 0 )
			return(rc);
		crc = updcrc16(type, crc);

		for( n = 4; --n >= 0; ++hdr )
		{
			if( (rc = zsendline((*hdr) & 0377)) < 0 )
				return(rc);
			crc = updcrc16(((*hdr) & 0377), crc);
		}
		
		crc = updcrc16(0,updcrc16(0,crc));
		if( (rc = zsendline((crc>>8) & 0377)) < 0 )
			return(rc);
		if( (rc = zsendline((crc   ) & 0377)) < 0 )
			return(rc);
	}
	
	if( (rc = FLUSHBUF()) < 0 )
		return(rc);
	if( type != ZDATA && (rc = FLUSHOUT()) < 0 )
		return rc;
	
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Send ZMODEM HEX header hdr of type $type                                  */
/* ------------------------------------------------------------------------- */
int zshhdr(int type, char *hdr)
{
	int n, rc;
	unsigned short crc;

	DEB((D_PROT, "zshhdr: %s %lx (CRC16)",
		FrameTypes[(type & 0x7f)+FTOFFSET], rclhdr(hdr)));

	Z_Txcrc32 = 0;
	
	if( (rc = BUFCHAR(ZPAD)) < 0 ) return(rc);
	if( (rc = BUFCHAR(ZPAD)) < 0 ) return(rc);
	if( (rc = BUFCHAR(ZDLE)) < 0 ) return(rc);
	if( (rc = BUFCHAR(ZHEX)) < 0 ) return(rc);
	if( (rc = zputhex(type)) < 0 ) return(rc);

	crc = updcrc16(type, 0);
	for( n = 4; --n >= 0; ++hdr )
	{
		if( (rc = zputhex(*hdr & 0377)) < 0 )
			return(rc);
		crc = updcrc16((*hdr & 0377), crc);
	}
	
	crc = updcrc16(0, updcrc16(0,crc));
	if( (rc = zputhex((crc>>8)&0377)) < 0 ) return(rc);
	if( (rc = zputhex((crc   )&0377)) < 0 ) return(rc);

	/* Make it printable on remote machine */
	if( (rc = BUFCHAR('\r')) < 0 ) return(rc);
	if( (rc = BUFCHAR('\n')) < 0 ) return(rc);

	/* Uncork the remote in case a fake XOFF has stopped data flow */
	if( type != ZFIN && type != ZACK )
	{
		if( (rc = BUFCHAR(XON)) < 0 )
			return(rc);
	}
	
	if( (rc = FLUSHBUF()) < 0 ) return(rc);
	if( (rc = FLUSHOUT()) < 0 ) return(rc);
	
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Send binary array buf of length length, with ending ZDLE sequence frameend*/
/* $pos must point to current offset of sending file (for logging only)      */
/* ------------------------------------------------------------------------- */
#ifdef DEBUG
static const char *Zendnames[] = {"ZCRCE", "ZCRCG", "ZCRCQ", "ZCRCW"};
#endif

int zsdata(const char *buf, int length, int frameend, long pos)
{
	int n, rc;
	unsigned short crc;
	unsigned long crc32;

	DEB((D_PROT, "zsdata: %d %s (CRC%s) from %d pos", length,
		Zendnames[(frameend-ZCRCE)&3], Z_Txcrc32?"32":"16", pos));
		
	if( Z_Txcrc32 )
	{
		crc32 = 0xFFFFFFFFL;
		
		for( ; --length >= 0; ++buf )
		{
			if( (rc = zsendline((*buf) & 0377)) < 0 )
				return(rc);
			crc32 = updcrc32(((*buf) & 0377), crc32);
		}
		
		if( (rc = BUFCHAR(ZDLE)) < 0 )
			return(rc);
		if( (rc = BUFCHAR(frameend)) < 0 )
			return(rc);
		crc32 = updcrc32(frameend, crc32);

		crc32 = ~crc32;
		for( n = 4; --n >= 0; )
		{
			if( (rc = zsendline(crc32 & 0377)) < 0 )
				return(rc);
			crc32 >>= 8;
		}
	}
	else /* CRC-16 */
	{
		crc = 0;
		for( ; --length >= 0; ++buf )
		{
			if( (rc = zsendline((*buf) & 0377)) < 0 )
				return(rc);
			crc = updcrc16((*buf & 0377), crc);
		}
		
		if( (rc = BUFCHAR(ZDLE)) < 0 )
			return(rc);
		if( (rc = BUFCHAR(frameend)) < 0 )
			return(rc);
		crc = updcrc16(frameend, crc);

		crc = updcrc16(0, updcrc16(0, crc));
		if( (rc = zsendline((crc>>8) & 0377)) < 0
		 || (rc = zsendline((crc   ) & 0377)) < 0 )
			return(rc);
	}
	
	if( (rc = FLUSHBUF()) < 0 )
		return(rc);
	
	if( frameend == ZCRCW )
	{
		if( (rc = PUTCHAR(XON)) < 0 )
			return(rc);
		if( (rc = FLUSHOUT())   < 0 )
			return(rc);
	}
	
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Receive array buf of max length+1 with ending ZDLE sequence               */
/* and CRC.  Returns the ending character or error code.                     */
/* $pos must contatain current offset of receiving file (for logging only!)  */
/* ------------------------------------------------------------------------- */
int zrdata(char *buf, int length, long pos)
{
	return (Z_Rxframeind == ZBIN32) ? zrdat32(buf, length, pos)
	                                : zrdat16(buf, length, pos);
}

static int zrdat16(char *buf, int length, long pos)
{
	int c;
	unsigned short crc;
	char *end;
	int d;

	Z_Rxcount = 0;
	crc = 0;
	end = buf + length;
	
	while( buf <= end )
	{
		if( (c=zdlread()) & ~0377 )
		{
switch_again:
			switch( c ) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				d = c;
				c &= 0377;
				crc = updcrc16(c, crc);
					
				if( (c=zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc16(c, crc);
					
				if( (c=zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc16(c, crc);
					
				Z_Rxcount = length - (end - buf);
				
				if( crc & 0xFFFF )
				{
					DEB((D_PROT, "zrdat16: %d %s : Bad CRC",
						Z_Rxcount, Zendnames[(d-GOTCRCE)&3]));;
					log("ZDATA with bad CRC");
					
					return ZCRCERR;
				}

				DEB((D_PROT, "zrdat16: %d %s (CRC16) at %d pos",
					Z_Rxcount, Zendnames[(d-GOTCRCE)&3], pos));
					    
				return d;
				
			case GOTCAN:
				DEB((D_PROT, "zrdat16: Sender CANceled"));
				return ZCAN;
				
			case ZHANGUP:
			case ZTIMER:
			case ZEXIT:
				DEB((D_PROT, "zrdat16: zdlread() return %s",
					( c == ZHANGUP ) ? "ZHANGUP":
					( c == ZEXIT   ) ? "ZEXIT":
					( c == ZTIMER  ) ? "ZTIMER" : "Unknown"));
				return c;
				
			default:
				DEB((D_PROT, "zrdat16: $zdlread return %d", c));
				return c;
			}
		}
		*buf++ = c;
		crc = updcrc16(c, crc);
	}
	
	log("zmodem: data packet too long");
	DEB((D_PROT, "zrdat16: data packet too long"));
	
	return ZERROR;
}

static int zrdat32(char *buf, int length, long pos)
{
	int c, d;
	unsigned long crc;
	char *end;

	Z_Rxcount = 0;
	crc = 0xFFFFFFFFL;
	end = buf + length;
	
	while( buf <= end )
	{
		if( (c = zdlread()) & ~0377 )
		{
switch_again:
			switch( c ) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				d = c;
				c &= 0377;
				crc = updcrc32(c, crc);
				if( (c = zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc32(c, crc);
				
				if( (c = zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc32(c, crc);
				
				if( (c = zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc32(c, crc);
				
				if( (c = zdlread()) & ~0377 ) goto switch_again;
				crc = updcrc32(c, crc);
				
				Z_Rxcount = length - (end - buf);
				
				if( crc != 0xDEBB20E3 )
				{
					DEB((D_PROT, "zrdat32: %d %s : Bad CRC",
						Z_Rxcount, Zendnames[(d-GOTCRCE)&3]));
					log("DATA with bad CRC");
					
					return ZCRCERR;
				}

				DEB((D_PROT, "zrdat32: %d %s (CRC32) at %d pos",
					Z_Rxcount, Zendnames[(d-GOTCRCE)&3], pos));

				return d;
				
			case GOTCAN:
				DEB((D_PROT, "zrdat32: Sender CANceled"));				    
				return ZCAN;
				
			case ZEXIT:
			case ZTIMER:
			case ZHANGUP:
				DEB((D_PROT, "zrdat32: zdlread() return %s",
					( c == ZHANGUP ) ? "ZHANGUP":
					( c == ZEXIT   ) ? "ZEXIT":
					( c == ZTIMER  ) ? "ZTIMER" : "Unknown"));
				return c;
				
			default:
				DEB((D_PROT, "zrdat32: $zdlread return %d", c));
				return c;
			}
		}
		*buf++ = c;
		crc = updcrc32(c, crc);
	}
	
	log("zmodem: data packet too long");
	DEB((D_PROT, "zrdat32: data packet too long"));
	
	return ZERROR;
}

/* ------------------------------------------------------------------------- */
/* Read a ZMODEM header to hdr, either binary or hex.                        */
/* On success, set Z_Rxpos and return type of header.                        */
/* Otherwise return negative on error.                                       */
/* Return ZERROR instantly if ZCRCW sequence, for fast error recovery.       */
/* ------------------------------------------------------------------------- */
int zgethdr(char *hdr)
{
	int c, n, cancount;

	n        = 8192;    /* Maximum number of garbage chars */
	cancount = 5;       /* CANcel on this can number */

	Z_Rxframeind = 0;
	Z_Rxtype     = 0;
	
again:
	if( Z_Rxexp ) { c = ZTIMER; goto fifi; }
	
	switch( c = GETCHAR(Z_Rxwait) ) {
	case ZEXIT:
	case ZHANGUP:
	case ZTIMER:
		goto fifi;
	case CAN:
gotcan:
		if( --cancount == 0 )
		{
			c = ZCAN; goto fifi;
		}
		
		if( Z_Rxexp ) { c = ZTIMER; goto fifi; }
		
		switch( c = GETCHAR(Z_Rxwait) ) {
		case ZCRCW:
			/* Return immediate ERROR if ZCRCW sequence seen */
			c = ZERROR;
		case ZEXIT:
		case ZHANGUP:
		case ZTIMER:
			goto fifi;
		default:
			break;
		case CAN:
			if( --cancount == 0 )
			{
				c = ZCAN; goto fifi;
			}
			goto again;
		}
	default:
		if( --n == 0 ) {
			DEB((D_PROT, "zgethdr: garbage count exceeded"));
			c = ZERROR; goto fifi;
		}
		cancount = 5;
		goto again;
	case ZPAD|0200:		/* This is what we want. */
	case ZPAD:		/* This is what we want. */
		break;
	}
	cancount = 5;
	
splat:
	switch( c = noxrd7() ) {
	case ZPAD:
		goto splat;
	case ZEXIT:
	case ZHANGUP:
	case ZTIMER:
		goto fifi;
	default:
		if( --n == 0 ) {
			DEB((D_PROT, "zgethdr: garbage count exceeded"));
			c = ZERROR; goto fifi;
		}
		goto again;
	case ZDLE:		/* This is what we want. */
		break;
	}

	switch( c=noxrd7() ) {
	case ZEXIT:
	case ZHANGUP:
	case ZTIMER:
		goto fifi;
	case ZBIN:
		/* receive binary header with crc16 */
		Z_Rxcrc32    = 0;
		Z_Rxframeind = ZBIN;
		c = zrbhdr16(hdr);
		break;
	case ZBIN32:
		/* receive binary header with crc32 */
		Z_Rxcrc32    = 1;
		Z_Rxframeind = ZBIN32;
		c = zrbhdr32(hdr);
		break;
	case ZHEX:
		/* receive hex header (all hex headers have crc16) */
		Z_Rxcrc32    = 0;
		Z_Rxframeind = ZHEX;
		c = zrhhdr(hdr);
		break;
	case CAN:
		goto gotcan;
	default:
		if( --n == 0 ) {
			DEB((D_PROT, "zgethdr: garbage count exceeded"));
			c = ZERROR; goto fifi;
		}
		goto again;
	}
	
	if( c >= 0 )
	{
		Z_Rxpos = hdr[ZP3] & 0377;
		Z_Rxpos = (Z_Rxpos<<8) + (hdr[ZP2] & 0377);
		Z_Rxpos = (Z_Rxpos<<8) + (hdr[ZP1] & 0377);
		Z_Rxpos = (Z_Rxpos<<8) + (hdr[ZP0] & 0377);
	}

fifi:
    	if( c == GOTCAN )
		c = ZCAN;
	
#ifdef DEBUG
	if( (c >= -7) && (c <= FRTYPES) )
		DEB((D_PROT, "zgethdr: %s %lx", FrameTypes[c+FTOFFSET], Z_Rxpos));
	else
		DEB((D_PROT, "zgethdr: %d %lx", c, Z_Rxpos));
#endif

	return(c);
}

/* ------------------------------------------------------------------------- */
/* Receive a binary style header (type and position)                         */
/* ------------------------------------------------------------------------- */
static int zrbhdr16(char *hdr)
{
	int c, n;
	unsigned short crc;

	if( (c = zdlread()) < 0 )
		return(c);
	Z_Rxtype = c;
	crc = updcrc16(c, 0);

	for( n = 4; --n >= 0; ++hdr )
	{
		if( (c = zdlread()) < 0 )
			return(c);
		crc = updcrc16(c, crc);
		*hdr = c;
	}
	
	if( (c = zdlread()) < 0 )
		return(c);
	crc = updcrc16(c, crc);
	
	if( (c = zdlread()) < 0 )
		return(c);
	crc = updcrc16(c, crc);
	
	if( crc & 0xFFFF )
	{
		DEB((D_PROT, "zrbhdr: Bad CRC"));
		log("Bad CRC"); 
		return(ZCRCERR);
	}
	
	return(Z_Rxtype);
}

/* ------------------------------------------------------------------------- */
/* Receive a binary style header (type and position) with 32 bit FCS         */
/* ------------------------------------------------------------------------- */
static int zrbhdr32(char *hdr)
{
	int c, n;
	unsigned long crc;

	if( (c = zdlread()) < 0 )
		return c;
	Z_Rxtype = c;
	crc = 0xFFFFFFFFL;
	crc = updcrc32(c, crc);

	for( n = 4; --n >= 0; ++hdr )
	{
		if( (c = zdlread()) < 0 )
			return(c);
		crc = updcrc32(c, crc);
		*hdr = c;
	}
	
	for( n = 4; --n >= 0; )
	{
		if( (c = zdlread()) < 0 )
			return(c);
		crc = updcrc32(c, crc);
	}
	
	if( crc != 0xDEBB20E3 )
	{
		DEB((D_PROT, "zrbhdr32: Bad CRC"));
		log("Bad CRC");
		return(ZCRCERR);
	}
	
	return(Z_Rxtype);
}


/* ------------------------------------------------------------------------- */
/* Receive a hex style header (type and position)                            */
/* ------------------------------------------------------------------------- */
static int zrhhdr(char *hdr)
{
	int c, n;
	unsigned short crc;

	if( (c = zgethex()) < 0 )
		return(c);
	Z_Rxtype = c; crc = updcrc16(c, 0);

	for( n = 4; --n >= 0; )
	{
		if( (c = zgethex()) < 0 )
			return(c);
		crc = updcrc16(c, crc);
		*hdr++ = c;
	}

	if( (c = zgethex()) < 0 )
		return(c);
	crc = updcrc16(c, crc);
	
	if( (c = zgethex()) < 0 )
		return(c);
	crc = updcrc16(c, crc);
	
	if( crc & 0xFFFF )
	{
		DEB((D_PROT, "zrhhdr: Bad CRC"));
		log("Bad CRC");
		return(ZCRCERR);
	}
	
	if( CHARWAIT(0) )
	{
		/* There is some characters available.. */
		
		switch( (c = GETCHAR(1)) ) {
		case 0215:
		case 015:
	 		/* Throw away possible cr/lf */
			if( (c = GETCHAR(1)) < 0 && c != ZTIMER )
				return(c);
			break;
		case ZEXIT:
		case ZHANGUP:
			return(c);
		}
	}
	
	return(Z_Rxtype);
}

/* ------------------------------------------------------------------------- */
/* Write a byte as two hex digits                                            */
/* ------------------------------------------------------------------------- */
static int zputhex(int c)
{
	static char digits[] = "0123456789abcdef";
	int rc;

	if( (rc = BUFCHAR(digits[(c&0xF0)>>4])) < 0
	 || (rc = BUFCHAR(digits[(c&0x0F)   ])) < 0 )
		return rc;
	
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Init an array of 256 entries, mark codes we must escape with value > 0    */
/* ------------------------------------------------------------------------- */
static void zsendline_init(char *tab)
{
	int i;
	
	for( i = 0; i < 256; i++ )
	{	
		if( i & 0140 )
			tab[i] = 0;
		else
		{
			switch( i ) {
			case ZDLE:
			case XOFF: /* ^Q */
			case XON:  /* ^S */
			case 020:  /* ^P */
			case (XOFF | 0200):
			case (XON  | 0200):
			case (020  | 0200):
				tab[i] = 1;
				break;
			case 015:
			case (015 | 0200):
				if( Z_Ctlesc )
					tab[i] = 1;
				else 
					tab[i] = 2;
				break;
			default:
				if( Z_Ctlesc )
					tab[i] = 1;
				else
					tab[i] = 0;
			}
		} /* end of if */
	} /* end of for */
}

/* ------------------------------------------------------------------------- */
/* Decode two lower case hex digits into an 8 bit byte value                 */
/* ------------------------------------------------------------------------- */
static int zgethex(void)
{
	int c, n;

	if( (c = noxrd7()) < 0 )
		return(c);
	
	n = c - '0';
	if( n > 9 ) n -= ('a' - ':');
	
	if( n & ~0xF )
		return(ZERROR);
	if( (c = noxrd7()) < 0 )
		return(c);
	
	c -= '0';
	if( c > 9 ) c -= ('a' - ':');
	if( c & ~0xF )
		return(ZERROR);
	
	c += (n<<4);
	
	return(c);
}

/* ------------------------------------------------------------------------- */
/* Store long integer pos in hdr                                             */
/* ------------------------------------------------------------------------- */
void stohdr(char *hdr, long pos)
{
	hdr[ZP0] = (pos    ) & 0377;
	hdr[ZP1] = (pos>>8 ) & 0377;
	hdr[ZP2] = (pos>>16) & 0377;
	hdr[ZP3] = (pos>>24) & 0377;
}

/* ------------------------------------------------------------------------- */
/* Recover a long integer from a header                                      */
/* ------------------------------------------------------------------------- */
long rclhdr(char *hdr)
{
	register long l;

	l = (hdr[ZP3] & 0377);
	l = (l << 8) | (hdr[ZP2] & 0377);
	l = (l << 8) | (hdr[ZP1] & 0377);
	l = (l << 8) | (hdr[ZP0] & 0377);
	return(l);
}
