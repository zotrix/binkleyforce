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
#include "session.h"
#include "outbound.h"
#include "prot_common.h"
#include "prot_binkp.h"

/*****************************************************************************
 * Initialise binkp state structure, allocate memory for buffers, etc.
 *
 * Arguments:
 * 	bpi       pointer to the binkp state structure (s_bpinfo)
 *
 * Return value:
 * 	None
 */
void binkp_init_bpinfo(s_bpinfo *bpi)
{
	memset(bpi, '\0', sizeof(s_bpinfo));

	bpi->obuf  = (char*)xmalloc(BINKP_MAX_BLKSIZE + BINKP_BLK_HDRSIZE + 1);
	bpi->opos  = 0;
	bpi->optr  = bpi->obuf;
	
	bpi->inew  = TRUE;
	bpi->isize = -1;
	bpi->ibuf  = (char*)xmalloc(BINKP_MAX_BLKSIZE + BINKP_BLK_HDRSIZE + 1);

	bpi->timeout = conf_number(cf_binkp_timeout);
	if( !bpi->timeout )
		bpi->timeout = 300;
}

/*****************************************************************************
 * DeInitialise binkp state structure, release allocated memory, etc.
 *
 * Arguments:
 * 	bpi       pointer to the binkp state structure (s_bpinfo)
 *
 * Return value:
 * 	None
 */
void binkp_deinit_bpinfo(s_bpinfo *bpi)
{
	if( bpi->obuf )
		free(bpi->obuf);
	if( bpi->ibuf )
		free(bpi->ibuf);
	if( bpi->msgqueue )
		free(bpi->msgqueue);
	
	memset(bpi, '\0', sizeof(s_bpinfo));
}

/*****************************************************************************
 * Fills s[0] and s[1] with binkp frame header using value of u
 *
 * Arguments:
 * 	s         pointer to the binkp's frame header (2 bytes)
 * 	val       value to put
 *
 * Return value:
 * 	None
 */
void binkp_puthdr(char *s, unsigned int val)
{
	s[0] = ( (unsigned long) val >> 8 ) & 0xff;
	s[1] = ( (unsigned long) val      ) & 0xff;
}

/*****************************************************************************
 * Fills s[0] with binkp message type value
 *
 * Arguments:
 * 	s         pointer to the destination buffer
 * 	msg       message type
 *
 * Return value:
 * 	None
 */
void binkp_putmsgtype(char *s, e_bpmsg msg)
{
	s[0] = ( (unsigned int) msg ) & 0xff;
}

/*****************************************************************************
 * Extract size from binkp frame header
 *
 * Arguments:
 * 	s         pointer to the source buffer
 *
 * Return value:
 * 	Value, extracted from binkp header
 */
int binkp_gethdr(const char *s)
{
	return ( (unsigned int) (((unsigned char) s[0]) & 0x7f) << 8 )
	     | ( (unsigned int) (((unsigned char) s[1]) & 0xff)      );
}

/*****************************************************************************
 * Extract message type from s[0]
 *
 * Arguments:
 * 	s         pointer to the source buffer (1 byte)
 *
 * Return value:
 * 	Value, extracted from binkp header, or -1 if extracted message type is
 * 	out of range
 */
int binkp_getmsgtype(char ch)
{
	int val = ( ((unsigned char)ch) & 0x7f );
	
	if( BPMSG_MIN > val || val > BPMSG_MAX )
		return -1;
	else
		return val;
}

/*****************************************************************************
 * Puts a message to the output messages queue. These msgs will be send
 * right after the current data block
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	msg       message type
 * 	s1        message text
 * 	s2        message text (will be concatenated with s1)
 *
 * Return value:
 * 	None
 */
void binkp_queuemsg(s_bpinfo *bpi, e_bpmsg msg, const char *s1, const char *s2)
{
	bpi->msgqueue = xrealloc(bpi->msgqueue, sizeof(s_bpmsg)*(bpi->n_msgs + 1));
	memset(&bpi->msgqueue[bpi->n_msgs], '\0', sizeof(s_bpmsg));
	
	/* Set message type */
	bpi->msgqueue[bpi->n_msgs].type = msg;
	
	/* Set message data size (without frame header) */
	bpi->msgqueue[bpi->n_msgs].size = 1;
	if( s1 ) bpi->msgqueue[bpi->n_msgs].size += strlen(s1);
	if( s2 ) bpi->msgqueue[bpi->n_msgs].size += strlen(s2);
	
	/* Set message data */
	bpi->msgqueue[bpi->n_msgs].data = xmalloc(bpi->msgqueue[bpi->n_msgs].size+3);
	binkp_puthdr(bpi->msgqueue[bpi->n_msgs].data,
	             (unsigned)(bpi->msgqueue[bpi->n_msgs].size | 0x8000));
	
	binkp_putmsgtype(bpi->msgqueue[bpi->n_msgs].data + 2, msg);
	
	bpi->msgqueue[bpi->n_msgs].data[3] = '\0';
	if( s1 ) strcat(bpi->msgqueue[bpi->n_msgs].data + 3, s1);
	if( s2 ) strcat(bpi->msgqueue[bpi->n_msgs].data + 3, s2);

	DEB((D_PROT, "binkp_queuemsg: queued message #%d, %ld bytes, \"%s\"",
		(int)bpi->msgqueue[bpi->n_msgs].type,
		(long)bpi->msgqueue[bpi->n_msgs].size,
		(char*)bpi->msgqueue[bpi->n_msgs].data+3));

	++bpi->n_msgs;
	++bpi->msgs_in_batch;
}

/*****************************************************************************
 * Sends a message using format string
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	msg       message type
 * 	fmt       pointer to the format string, (man printf)
 *
 * Return value:
 * 	None
 */
void binkp_queuemsgf(s_bpinfo *bpi, e_bpmsg msg, const char *fmt, ...)
{
	char msg_text[2048];
	va_list ap;
	
	va_start(ap, fmt);
#ifdef HAVE_SNPRINTF
	vsnprintf(msg_text, sizeof(msg_text), fmt, ap);
#else
	vsprintf(msg_text, fmt, ap);
#endif
	va_end(ap);
	
	binkp_queuemsg(bpi, msg, msg_text, NULL);
}

/*****************************************************************************
 * Parse most common message arguments, send error message to remote
 * if needed.
 *
 * Arguments:
 * 	s         pointer to the string to parse
 * 	fn        pointer to the pointer for extracted file name
 * 	sz        pointer to the extracted file size
 * 	tm        pointer to the extracted file modification time
 * 	offs      pointer to the extracted file offset
 *
 * Return value:
 * 	non-zero value on error, zero on success
 */
int binkp_parsfinfo(char *str,char **fn,size_t *sz,time_t *tm,size_t *offs){

	char *n;
	char *p_fname = NULL;
	char *p_size  = NULL;
	char *p_time  = NULL;
	char *p_offs  = NULL;

	static char *s = NULL;

	/* Attention, offs may be NULL! */
	
	ASSERT(str != NULL && fn != NULL && sz != NULL && tm != NULL);

	DEB((D_PROT, "binkp_parsemsg: want parse \"%s\"", s));
	
	if (s) free (s);
	s = xstrcpy (str);
	p_fname = string_token(s, &n, NULL, 0);
	p_size  = string_token(NULL, &n, NULL, 0);
	p_time  = string_token(NULL, &n, NULL, 0);
	if( offs ) p_offs = string_token(NULL, &n, NULL, 0);
	
	if( p_fname && p_size && p_time && (!offs || p_offs) )
	{
		if( ISDEC(p_size) && ISDEC(p_time) &&
		    (!offs || ISDEC(p_offs) || !strcmp (p_offs, "-1")) )
		{
			(*fn) = p_fname;
			(*sz) = atol(p_size);
			(*tm) = atol(p_time);
			if( offs ) *offs = atol(p_offs);
			DEB((D_PROT, "binkp_parsemsg: file = \"%s\", size = %d, time = %d",
				*fn, *sz, *tm));
			return 0;
		}
	}
	return 1;
}

/*****************************************************************************
 * Put message to a buffer
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	msg       message
 *
 * Return value:
 * 	Zero on success, and non-zero value if no more free space left
 * 	in the buffer
 */
int binkp_buffer_message(s_bpinfo *bpi, s_bpmsg *msg)
{
	DEB((D_PROT, "binkp_buffer_message: buffer message #%d, %ld byte(s)",
		(int)msg->type, (long)msg->size));
	
	/* Check for possible internal error */
	if( msg->size > BINKP_MAX_BLKSIZE )
	{
		log("size of msg we want to send is too big (%db)", msg->size);
		return 1;
	}
	
	/* Is there space for the new msg? */
	if( bpi->opos + msg->size > BINKP_MAX_BLKSIZE ) return 1;
	
	if( msg->data )
	{
		memcpy(bpi->obuf + bpi->opos, msg->data, msg->size + BINKP_BLK_HDRSIZE);
		bpi->opos += msg->size + BINKP_BLK_HDRSIZE;
		free(msg->data);
		msg->data = NULL;
	}
	
	return 0;
}

/*****************************************************************************
 * Send as much buffered data as possible (non-blocking)
 *
 * Arguments:
 * 	bpi       binkp state structure
 *
 * Return value:
 * 	Zero on success, and non-zero value on errors
 */
int binkp_send_buffer(s_bpinfo *bpi)
{
	int n;

	DEB((D_PROT, "binkp_send_buffer: sending %ld byte(s)", (long)bpi->opos));
	
	n = tty_write(bpi->optr, bpi->opos);
	
	if( n >= bpi->opos )
		{ bpi->optr = bpi->obuf; bpi->opos = 0; }
	else if( n > 0 )
		{ bpi->optr += n; bpi->opos -= n; }
	else
		return -1;
	
	return 0;
}

/*****************************************************************************
 * Send as much buffered data or messages from the queue as possible
 *
 * Arguments:
 * 	bpi       binkp state structure
 *
 * Return value:
 * 	Zero on success, and non-zero value on errors
 */
int binkp_send(s_bpinfo *bpi)
{
	int i;
	
	if( bpi->opos == 0 && bpi->msgqueue )
	{
		/*
		 *  Buffer is empty and there are unsent msgs
		 */
		for( i = 0; i < bpi->n_msgs; i++ )
		{
			if( bpi->msgqueue[i].sent == FALSE )
			{
				if( binkp_buffer_message(bpi, &bpi->msgqueue[i]) )
					break;
				else
					bpi->msgqueue[i].sent = TRUE;
			}
		}
		/* If the message queue is empty, free it */
		if( i >= bpi->n_msgs )
		{
			if( bpi->msgqueue )
				{ free(bpi->msgqueue); bpi->msgqueue = NULL; }
			bpi->n_msgs = 0;
		}
	}

	return bpi->opos ? binkp_send_buffer(bpi) : 0;
}

/*****************************************************************************
 * Flush all buffers and queues that can contain unsent data
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	timeout   abort flushing if this time will be exceeded
 *
 * Return value:
 * 	Zero on success, and non-zero value on errors
 */
int binkp_flush_queue(s_bpinfo *bpi, int timeout)
{
	bool send_ready = FALSE;
	time_t flush_timer;
	
	timer_set(&flush_timer, timeout);
	
	while( bpi->opos > 0 || bpi->n_msgs > 0 )
	{
		if( timer_expired(flush_timer)
		 || tty_select(NULL, &send_ready, 10) < 0 )
			return -1;
		 
		if( send_ready && binkp_send(bpi) < 0 )
			return -1;
	}

	return 0;
}

/*****************************************************************************
 * Try to receive next message or data block in non-blocking mode
 *
 * Arguments:
 * 	bpi       binkp state structure
 *
 * Return value:
 * 	One of the BPMSG_* values
 */
int binkp_recv(s_bpinfo *bpi)
{
	int n = 0;
	int size;

	if( bpi->inew || bpi->isize == -1 )
	{
		bpi->inew  = FALSE;
		bpi->isize = -1;
		size = BINKP_BLK_HDRSIZE;
	}
	else
		size = bpi->isize;
	
	if( size > 0 )
	{
		n = tty_read(bpi->ibuf + bpi->ipos, size - bpi->ipos);
		if( n <= 0 ) return BPMSG_EXIT;
	}
	
	bpi->ipos += n;
	
	if( bpi->ipos == size )
	{
		if( bpi->isize == -1 )
		{
			/* We got block header */
			bpi->ipos  = 0;
			bpi->imsg  = ((bpi->ibuf[0] >> 7) & 0377) ? TRUE : FALSE;
			bpi->isize = binkp_gethdr(bpi->ibuf);
			DEB((D_PROT, "binkp_recv: received header: %ld byte(s) (%s)",
				(long)bpi->isize, bpi->imsg ? "msg" : "data"));
			if( bpi->isize > BINKP_MAX_BLKSIZE )
			{
				log("internal error: got %ld bytes block size", bpi->isize);
				return BPMSG_EXIT;
			}
			else if( bpi->isize == 0 )
			{
				bpi->ipos  = 0;
				bpi->inew  = TRUE;
				bpi->isize = -1;
				if( bpi->imsg == TRUE )
					log("zero length message from remote");
			}
		}
		else /* We got whole data block/message */
		{
			if( bpi->imsg == TRUE ) /* Is it message? */
			{
				bpi->ipos = 0;
				bpi->inew = TRUE;
				bpi->ibuf[size] = '\0';
				bpi->imsgtype = binkp_getmsgtype(bpi->ibuf[0]);
				DEB((D_PROT, "binkp_recv: got message #%d, %ld byte(s), \"%s\"",
					(int)bpi->imsgtype, bpi->isize, bpi->ibuf+1));
				++bpi->msgs_in_batch;
				if( bpi->imsgtype < 0 )
				{
					bpi->isize = -1;
					log("got incorrect message type");
					if( ++bpi->junkcount >= 5 )
					{
						log("junk count exceeded");
						return BPMSG_EXIT;
					}
					return BPMSG_NONE;
				}
				return bpi->imsgtype;
			}
			else /* It is data */
			{
				bpi->ipos = 0;
				bpi->inew = TRUE;
				bpi->ibuf[size] = '\0';
				DEB((D_PROT, "binkp_recv: got data block, %ld byte(s)",
					(long)bpi->isize));
				return BPMSG_DATA;
			}
		}
	}
	
	return BPMSG_NONE;
}

/*****************************************************************************
 * Send (queue) system information to the remote
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */
void binkp_queue_sysinfo(s_bpinfo *bpi, s_binkp_sysinfo *binkp)
{
	int i;
	char *astr = NULL;
	char abuf[BF_MAXADDRSTR+1];
	
	if( !state.caller && binkp->challenge_length > 0 )
	{
		char challenge[128];
		string_bin_to_hex(challenge, binkp->challenge, binkp->challenge_length);
		binkp_queuemsgf(bpi, BPMSG_NUL, "OPT CRAM-MD5-%s", challenge);
	}	
	
	binkp_queuemsg(bpi, BPMSG_NUL, "SYS ", binkp->systname);
	binkp_queuemsg(bpi, BPMSG_NUL, "ZYZ ", binkp->sysop);
	binkp_queuemsg(bpi, BPMSG_NUL, "LOC ", binkp->location);
	binkp_queuemsg(bpi, BPMSG_NUL, "PHN ", binkp->phone);
	binkp_queuemsg(bpi, BPMSG_NUL, "NDL ", binkp->flags);

	binkp_queuemsg(bpi, BPMSG_NUL, "TIME ", binkp->timestr);
	
	binkp_queuemsgf(bpi, BPMSG_NUL, "VER %s %s/%d.%d",
		binkp->progname, binkp->protname,
		binkp->majorver, binkp->minorver);

	for( i = 0; i < binkp->anum; i++ )
	{
		if( i ) astr = xstrcat(astr, " ");
		astr = xstrcat(astr, ftn_addrstr(abuf, binkp->addrs[i].addr));
	}
	
	if( astr )
	{
		binkp_queuemsg(bpi, BPMSG_ADR, NULL, astr);
		free(astr);
	}
	if (state.caller)
	{
		char *szOpt = xstrcpy (" MB");
#ifndef NETSPOOL
		if (!nodelist_checkflag (state.node.flags, "NR"))
			szOpt = xstrcat (szOpt, " NR");
#endif
		if (!nodelist_checkflag (state.node.flags, "ND"))
			szOpt = xstrcat (szOpt, " ND");
		if (*szOpt)
			binkp_queuemsg(bpi, BPMSG_NUL, "OPT", szOpt);
		free (szOpt);
	}
}

/*****************************************************************************
 * Write options to the log
 *
 * Arguments:
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */
void binkp_log_options(s_binkp_sysinfo *remote)
{
	if (remote->options & BINKP_OPT_MB) log ("We are in MB mode.");
	if (remote->options & BINKP_OPT_NR) log ("We are in NR mode.");
}

/*****************************************************************************
 * Write system information to the log
 *
 * Arguments:
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */
void binkp_log_sysinfo(s_binkp_sysinfo *binkp)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	
	if( binkp->anum )
		for( i = 0; i < binkp->anum; i++ )
		{
			log("   Address : %s", ftn_addrstr(abuf, binkp->addrs[i].addr));
		}
	
	if( *binkp->systname && *binkp->phone )
		log("    System : %s (%s)",
			string_printable(binkp->systname),
			string_printable(binkp->phone));
	else if( *binkp->systname )
		log("    System : %s",
			string_printable(binkp->systname));
	else if( *binkp->phone )
		log("     Phone : %s",
			string_printable(binkp->phone));

#ifdef BFORCE_LOG_PASSWD
	if( *binkp->passwd )
		log("  Password : %s", string_printable(binkp->passwd));
#endif

	if( *binkp->opt )
		log("   Options : %s", string_printable(binkp->opt));

	if( *binkp->sysop && *binkp->location )
		log("     SysOp : %s from %s",
			string_printable(binkp->sysop),
			string_printable(binkp->location));
	else if( *binkp->sysop )
		log("     SysOp : %s",
			string_printable(binkp->sysop));
	else if( *binkp->location )
		log("  Location : %s",
			string_printable(binkp->location));

	if( *binkp->progname )
	{
		log("    Mailer : %s (%s/%d.%d)",
			*binkp->progname ? string_printable(binkp->progname) : "?",
			*binkp->protname ? string_printable(binkp->protname) : "?",
			binkp->majorver, binkp->minorver);
	}
	if( *binkp->flags )
		log("     Flags : %s", string_printable(binkp->flags));
	if( *binkp->timestr )
		log("      Time : %s", string_printable(binkp->timestr));
}

/*****************************************************************************
 * Set our local system information 
 *
 * Arguments:
 * 	binkp       structure where to store system information
 * 	remote_addr remote main address (for setting best AKA)
 * 	caller      are we calling system?
 *
 * Return value:
 * 	None
 */
void binkp_set_sysinfo(s_binkp_sysinfo *binkp, s_faddr *remote_addr, bool caller)
{
	s_cval_entry *addr_ptr;
	s_cval_entry *hide_ptr;
	s_faddr *primary = NULL;
	
	const char *p_systname  = conf_string(cf_system_name);
	const char *p_location  = conf_string(cf_location);
	const char *p_sysop     = conf_string(cf_sysop_name);
	const char *p_phone     = conf_string(cf_phone);
	const char *p_flags     = conf_string(cf_flags);
	
	/* free previously allocated memory */
	if( binkp->addrs )
		free(binkp->addrs);

	memset(binkp, '\0', sizeof(s_binkp_sysinfo));
	
	/* Set best primary address */
	if( remote_addr )
	{
		primary = session_get_bestaka(*remote_addr);
		
		/* Add primary address */
		if( primary )
			session_addrs_add(&binkp->addrs, &binkp->anum, *primary);
	}

	/* Add other AKAs */
	for( addr_ptr = conf_first(cf_address); addr_ptr;
	     addr_ptr = conf_next(addr_ptr) )
	{
		for( hide_ptr = conf_first(cf_hide_our_aka); hide_ptr;
		     hide_ptr = conf_next(hide_ptr) )
		{
			if( !ftn_addrcomp(hide_ptr->d.falist.addr, addr_ptr->d.falist.addr) )
				break;
		}
		
		if( !hide_ptr && primary != &addr_ptr->d.falist.addr )
			session_addrs_add(&binkp->addrs, &binkp->anum,
					addr_ptr->d.falist.addr);
	}
	
	if( binkp->anum == 0 )
		log("warning: no addresses will be presented to remote");

	/* session password */
	if( caller )
		session_get_password(state.node.addr, binkp->passwd, sizeof(binkp->passwd));
	
	binkp->majorver = BINKP_MAJOR;
	binkp->minorver = BINKP_MINOR;
	
	strnxcpy(binkp->progname, BF_NAME"/"BF_VERSION"/"BF_OS, sizeof(binkp->progname));
	strnxcpy(binkp->protname, BINKP_NAME, sizeof(binkp->protname));
	
	strnxcpy(binkp->sysop,    p_sysop    ? p_sysop    : "Unknown", sizeof(binkp->sysop));
	strnxcpy(binkp->systname, p_systname ? p_systname : "Unknown", sizeof(binkp->systname));
	strnxcpy(binkp->location, p_location ? p_location : "Unknown", sizeof(binkp->location));
	strnxcpy(binkp->phone,    p_phone    ? p_phone    : "Unknown", sizeof(binkp->phone));
	strnxcpy(binkp->flags,    p_flags    ? p_flags    : "BINKP",   sizeof(binkp->flags));

	time_string_format(binkp->timestr, sizeof(binkp->timestr), "%Y/%m/%d %H:%M:%S", 0);
	
	/* Set session challenge string */
	if( !caller )
	{
		long rnd = (long)random();
		int  pid = ((int)getpid()) ^ ((int)random());
		long utm = (long)time(0);

		binkp->options |= BINKP_OPT_MD5;
		binkp->challenge[0] = (unsigned char)(rnd      );
		binkp->challenge[1] = (unsigned char)(rnd >> 8 );
		binkp->challenge[2] = (unsigned char)(rnd >> 16);
		binkp->challenge[3] = (unsigned char)(rnd >> 24);
		binkp->challenge[4] = (unsigned char)(pid      );
		binkp->challenge[5] = (unsigned char)(pid >> 8 );
		binkp->challenge[6] = (unsigned char)(utm      );
		binkp->challenge[7] = (unsigned char)(utm >> 8 );
		binkp->challenge[8] = (unsigned char)(utm >> 16);
		binkp->challenge[9] = (unsigned char)(utm >> 24);

		binkp->challenge_length = 10;
	}
}

void binkp_parse_options(s_binkp_sysinfo *binkp, char *options)
{
	char *p, *n;

	for( p = string_token(options, &n, NULL, 0); p;
	     p = string_token(NULL, &n, NULL, 0) )
	{
#ifndef NETSPOOL
		if( !strcmp(p, "NR") ) {
			binkp->options |= BINKP_OPT_NR;
		} else
#endif
		if( !strcmp(p, "MB") )
			binkp->options |= BINKP_OPT_MB;
		else if( !strcmp(p, "MPWD") )
			binkp->options |= BINKP_OPT_MPWD;
		else if( !strncmp(p, "CRAM-", 5) )
		{
			char *hash_types = p + 5;
			char *challenge = strchr(hash_types, '-');

			if( challenge )
			{
				char *pp, *nn;
				
				*challenge++ = '\0';
				for( pp = string_token(hash_types, &nn, "/", 0); pp;
				     pp = string_token(NULL, &nn, "/", 0) )
				{
					if( !strcmp(pp, "SHA1") )
						binkp->options |= BINKP_OPT_SHA1;
					else if( !strcmp(pp, "MD5") )
						binkp->options |= BINKP_OPT_MD5;
					else if( !strcmp(pp, "DES") )
						binkp->options |= BINKP_OPT_DES;
				}
				
				if( strlen(challenge) > (2*sizeof(binkp->challenge)) )
					log("binkp got too long challenge string");
				else
					binkp->challenge_length = string_hex_to_bin(
							binkp->challenge, challenge);
			}
			else
				log("binkp got invalid option: \"%s\"", string_printable(p));
		}
	} 
}

