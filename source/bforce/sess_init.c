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
#include "version.h"
#include "logger.h"
#include "util.h"
#include "io.h"
#include "session.h"

#define INTRO_MAX_SIZE        512
#define INTRO_MAX_LINES       10
#define OUTGOING_MAST_TIMER   60
#define OUTGOING_SYNC_TIMER   5
#define INCOMING_MAST_TIMER   60
#define INCOMING_SYNC_TIMER   2

/* ------------------------------------------------------------------------- */
/* Initiate outbound session and define handshake protocol                   */
/* On success: zero   (state.session is handshake type)                      */
/* On error: non-zero (state.session set to UNKNOWN)                         */
/* ------------------------------------------------------------------------- */
int session_init_outgoing()
{
	int    c = 0;
	int    tries = 0;
	int    pos_emsi = 0;
	int    pos_intro = 0;
	bool   canemsi = FALSE;
	bool   canyoohoo = FALSE;
	bool   canftsc = FALSE;
	bool   canintro = FALSE;
	char   buf_emsi[13];
	char   buf_intro[256];
	int    emsi_seq = 0;    /* start reading emsi sequence     */
	int    enqcount = 0;    /* number of ENQ received          */
	int    nakcount = 0;    /* number of NAK or 'C' received   */
	int    enq_need = 1;
	int    nak_need = 1;
	time_t mast_timer = 0;  /* master timer, seems to be 60sec */
	time_t sync_timer = 0;  /* resync every .. seconds         */
	long   options = conf_options(cf_options);
	int    intro_lines = 0;
	int    intro_count = 0;
	int    unexp_count = 0;

	state.session = SESSION_UNKNOWN;
	
	if( (options & OPTIONS_NO_EMSI) != OPTIONS_NO_EMSI )
		canemsi = TRUE;
	if( (options & OPTIONS_NO_YOOHOO) != OPTIONS_NO_YOOHOO )
		canyoohoo = TRUE;
	if( (options & OPTIONS_NO_FTS1) != OPTIONS_NO_FTS1 )
		canftsc = TRUE;
	if( (options & OPTIONS_NO_INTRO) != OPTIONS_NO_INTRO )
		canintro = TRUE;
	
	enq_need = canemsi ? 2 : 1;
	nak_need = (canemsi || canyoohoo) ? 2 : 1;
	
	/*
	 * Put CR until any character received
	 */
	if( PUTCHAR('\r') < 0 || FLUSHOUT() < 0 ) {
		log("error: output");
		return 1;
	}
	
	while( !CHARWAIT(1) )
	{
		if( ++tries > 15 )
		{
			log("too much tries waking remote");
			return 1;
		}
		
		if( PUTCHAR('\r') < 0 || FLUSHOUT() < 0 ) {
			log("error: output");
			return 1;
		}
	}
	
#ifdef DEBUG
	if( tries > 0 )
		DEB((D_HSHAKE, "tx_init: remote waked on %d try", tries));
#endif
	
	/*
	 * Safety is the 1st law
	 */
	*buf_emsi = '\0';
	*buf_intro = '\0';
	
	/*
	 * Start timers
	 */
	timer_set(&mast_timer, OUTGOING_MAST_TIMER);
	timer_set(&sync_timer, OUTGOING_SYNC_TIMER*2);

	/*
	 * Determine supported handshakes on called system
	 * (support for FTS-1, YooHoo, EMSI)
	 */
	while(1)
	{
		if( timer_expired(mast_timer) )
		{
			log("session initialisation timed out");
			DEB((D_HSHAKE, "handshake initialisation timed out"));
			
			return 1;
		}
		
		if( timer_expired(sync_timer) )
		{
			DEB((D_HSHAKE, "tx_sendsync: resyncing"));
			
			if( canemsi && PUTSTR("**EMSI_INQC816**EMSI_INQC816") < 0 ) {
				log("error: output");
				return 1;
			}
			if( canyoohoo && PUTCHAR(YOOHOO) < 0 ) {
				log("error: output");
				return 1;
			}
			if( canftsc && PUTCHAR(TSYNC) < 0 ) {
				log("error: output");
				return 1;
			}
			if( canemsi && PUTCHAR('\r') < 0 ) {
				log("error: output");
				return 1;
			}

			if( FLUSHOUT() < 0 ) {
				log("error: flush");
				return 1;
			}
			
			timer_set(&sync_timer, OUTGOING_SYNC_TIMER);
		}
		
		/*
		 * Pickup next char
		 */
		c = GETCHAR(1);
		
		if( canintro && c > 0 )
		{
			if( c == XON || c == XOFF )
			{
				/* Do nothing. Drop them down */
			}
			else if( c == '\r' || c == '\n' )
			{
				if( pos_intro > 0 )
				{
					intro_lines += 1;
					intro_count += pos_intro;
					
					recode_intro_in(buf_intro);
					log("intro: \"%s\"",
						string_printable(buf_intro));
					
					pos_intro = 0;
					buf_intro[0] = '\0';
				}
			}
			else if( pos_intro < sizeof(buf_intro) - 1 )
			{
				buf_intro[pos_intro++] = (char)c;
				buf_intro[pos_intro  ] = '\0';
			}
			
			if( pos_intro >= sizeof(buf_intro) - 1 )
			{
				intro_lines += 1;
				intro_count += pos_intro;
				
				recode_intro_in(buf_intro);
				log("intro buffer is full");
				log("intro: \"%s\"",
					string_printable(buf_intro));
				
				pos_intro = 0;
				buf_intro[0] = '\0';
			}

			if( intro_lines >= INTRO_MAX_LINES )
			{
				log("stop logging intro: %d lines limit", intro_lines);
				canintro = FALSE;
			}
			else if( intro_count >= INTRO_MAX_SIZE )
			{
				log("stop logging intro: %d bytes limit", intro_count);
				canintro = FALSE;
			}
		}
		
		if( c < 0 )
		{
			if( c != TTY_TIMEOUT )
			{
				DEB((D_HSHAKE, "tx_init: got TTY_ERROR/TTY_HANGUP"));
				return 1;
			}				
		}
		else if( c == XON || c == XOFF )
		{
			/* Do nothing. Drop them down */
		}
		else if( c == ENQ )
		{
			if( enq_need && ++enqcount >= enq_need && canyoohoo )
			{
				DEB((D_HSHAKE, "tx_init: exit with YooHoo"));
				state.session = SESSION_YOOHOO;
				return 0;
			}
		}
		else if( c == TSYNC )
		{
			if( nak_need && ++nakcount > nak_need && canftsc )
			{
				DEB((D_HSHAKE, "tx_init: exit with FTS-1"));
				state.session = SESSION_FTSC;
				return 0;
			}
		}
		else if( canemsi && c > ' ' && c < '~' )
		{
			if( c != 'C' )
				nakcount = 0;
			
			enqcount = 0;
			
			if( c == '*' )
			{
				memset(buf_emsi, '\0', sizeof(buf_emsi));
				pos_emsi = 0;
				emsi_seq = 1;
			}
			else if( emsi_seq )
			{
				if( pos_emsi < sizeof(buf_emsi)-1 )
				{
					buf_emsi[pos_emsi++] = (char)c;
					buf_emsi[pos_emsi  ] = '\0';
				}
			
				if( pos_emsi >= sizeof(buf_emsi)-1 )
				{
					emsi_seq = 0;
					
					DEB((D_HSHAKE, "tx_init: emsi buffer full \"%s\"",
							string_printable(buf_emsi)));
					
					if( !strncasecmp(buf_emsi, "EMSI_REQA77E", 12)
					 || !strncasecmp(buf_emsi, "EMSI_NAKEEC3", 12) )
					{
						DEB((D_HSHAKE, "tx_init: exit with EMSI"));
						state.session = SESSION_EMSI;
						
						if( PUTSTR("**EMSI_INQC816\r") < 0
						 || PUTSTR("**EMSI_INQC816\r") < 0 ) {
							log("error: output");
							return 1;
						}
						if( FLUSHOUT() < 0 ) {
							log("error: output");
							return 1;
						}
						
						return 0;
					}
					else if( !strncasecmp(buf_emsi, "EMSI_INQC816", 12) )
					{
						/*
						 * Most probable it is echo of
						 * our own EMSI_INQ, try to send
						 * separated EMSI_INQ as user
						 * name and password to be sure
						 * that they understand us.
						 */

						/* Wait for a login prompt */
						sleep(3);
						
						if( PUTSTR("**EMSI_INQC816\r") < 0
						 || FLUSHOUT() < 0 ) {
							log("error: flush");
							return 1;
						}

						/* Wait for a password prompt */
						sleep(2);
						
						if( PUTSTR("**EMSI_INQC816\r") < 0
						 || FLUSHOUT() < 0 ) {
							log("error: output");
							return 1;
						}
						
						timer_set(&sync_timer, OUTGOING_SYNC_TIMER);
					}
					else if( !strncasecmp(buf_emsi, "EMSI", 4) )
					{
						log("unexpected emsi sequence \"%s\"",
								string_printable(buf_emsi));
						
						if( ++unexp_count > 10 )
						{
							log("too many unexpected emsi sequences");
							return 1;
						}
					}
				}
			}
		}
		else if( emsi_seq )
		{
			emsi_seq = 0;
			DEB((D_HSHAKE, "sm_rx_waitseq: bad character 0x%02x in \"%s\"",
					c, string_printable(buf_emsi)));
		}
	}
	log("session_init_outgoing: end loop");

	return 1;
}

/* ------------------------------------------------------------------------- */
/* Initiate inbound session and define handshake protocol                    */
/* On success: return zero (state.session defines handshake type)            */
/* On error: return non-zero (state.session set to HSHAKE_UNKNOWN)           */
/*                                                                           */
/* TODO: It is not working yet, it only reports about EMSI requests.. (1)    */
/* ------------------------------------------------------------------------- */
int session_init_incoming()
{
	int    c = 0;
	int    pos = 0;
	bool   canemsi = FALSE;
	bool   canyoohoo = FALSE;
	bool   canftsc = FALSE;
	char   buf[13];
	int    emsi_seq = 0;      /* start reading emsi sequence     */
	int    yoohoo_count = 0;  /* number of ENQ received          */
	int    tsync_count  = 0;  /* number of NAK or 'C' received   */
	int    yoohoo_need = 1;
	int    tsync_need = 1;
	time_t mast_timer = 0;    /* master timer, seems to be 60sec */
	time_t sync_timer = 0;
	long   options = conf_options(cf_options);
	int    unexp_count = 0;

	state.session = SESSION_UNKNOWN;
	
	log("init");
	
	if( (options & OPTIONS_NO_EMSI) != OPTIONS_NO_EMSI ) {
		log("can emsi");
		canemsi = TRUE;
	}
	if( (options & OPTIONS_NO_YOOHOO) != OPTIONS_NO_YOOHOO ) {
		log("can yahoo");
		canyoohoo = TRUE;
	}
	if( (options & OPTIONS_NO_FTS1) != OPTIONS_NO_FTS1 ) {
		log("can ftsc");
		canftsc = TRUE;
	}
	
	yoohoo_need = canemsi ? 2 : 1;
	tsync_need = (canemsi || canyoohoo) ? 2 : 1;

	if( PUTCHAR('\r') < 0 ) {
		log("error: cannot put char");
		return 1;
	}
	
	/*
	 * Output banner
	 */
	if( canemsi && PUTSTR("**EMSI_REQA77E\r") < 0 ) {
		log("error: cannot put banner");
		return 1;
	}

	if( state.connstr )
	{
		/* Show connect string */
		if( PUTCHAR('[') < 0 ) {
			log("error: cannot put ']'");
			return 1;
		}
		if( PUTSTR(state.connstr) < 0 ) {
			log("error: cannot put connstr");
			return 1;
		}
		if( PUTSTR("]\n") < 0 ) {
			log("error: cannot put ']'");
			return 1;
		}
	}
	
	if( PUTSTR(BF_BANNERVER) < 0 || PUTCHAR(' ') < 0
	 || PUTSTR(BF_COPYRIGHT) < 0 || PUTCHAR('\n') < 0 ) {
		log("session_init_incoming error: output");
		return 1;
	}

	if( FLUSHOUT() < 0 ) {
		log("session_init_incoming error: flush");
		return 1;
	}
	
	/* Start timers */
	timer_set(&mast_timer, INCOMING_MAST_TIMER);
	timer_set(&sync_timer, INCOMING_SYNC_TIMER/2);
	
	/*
	 * Determine supported handshakes on called system
	 * (support for FTS-1, YooHoo, EMSI)
	 */
	 
	log("begin loop");
	while(1)
	{
		if( timer_expired(mast_timer) )
		{
			log("session initialisation timed out");
			DEB((D_HSHAKE, "handshake initialisation timed out"));
			
			return 1;
		}
		
		if( timer_expired(sync_timer) )
		{
			DEB((D_HSHAKE, "rx_init: resyncing"));
			
			if( canemsi && PUTSTR("**EMSI_REQA77E\r") < 0 ) {
				log("session_init_incoming error: output");
				return 1;
			}

			if( FLUSHOUT() < 0 ) {
				log("session_init_incoming error: flush");
				return 1;
			}
			
			timer_set(&sync_timer, INCOMING_SYNC_TIMER);
		}
		
		/*
		 * Pickup next char
		 */
		if( (c = GETCHAR(1)) < 0 )
		{
			if( c != TTY_TIMEOUT )
			{
				DEB((D_HSHAKE, "rx_init: got TTY_ERROR/TTY_HANGUP"));
				return 1;
			}				
		}
		else if( c == XON || c == XOFF )
		{
			/* Do nothing. Drop them down */
		}
		else if( c == YOOHOO )
		{
			if( ++yoohoo_count >= yoohoo_need && canyoohoo )
			{
				DEB((D_HSHAKE, "rx_init: exit with YooHoo"));
				state.session = SESSION_YOOHOO;
				
				return 0;
			}
		}
		else if( c == TSYNC )
		{
			if( ++tsync_count > tsync_need && canftsc )
			{
				DEB((D_HSHAKE, "rx_init: exit with FTS-1"));
				state.session = SESSION_FTSC;
				
				return 0;
			}
		}
		else if( canemsi && c > ' ' && c < '~' )
		{
			tsync_count = 0;
			yoohoo_count = 0;
			
			if( c == '*' )
			{
				memset(buf, '\0', sizeof(buf));
				pos = 0;
				emsi_seq = 1;
			}
			else if( emsi_seq )
			{
				if( pos < sizeof(buf)-1 )
				{
					buf[pos++] = (char)(c & 0xff);
					buf[pos  ] = '\0';
				}
			
				if( pos >= sizeof(buf)-1 )
				{
					emsi_seq = 0;
					
					DEB((D_HSHAKE, "rx_init: emsi buffer full \"%s\"",
							string_printable(buf)));
					
					if( !strncasecmp(buf, "EMSI_INQC816", 12) )
					{
						DEB((D_HSHAKE, "rx_init: exit with EMSI"));
						state.session = SESSION_EMSI;
						
						return 0;
					}
					else if( !strncasecmp(buf, "EMSI", 4) )
					{
						log("unexpected emsi sequence \"%s\"",
								string_printable(buf));
						
						if( ++unexp_count > 10 )
						{
							log("too many unexpected emsi sequences");
							return 1;
						}
					}
				}
			}
		}
		else if( emsi_seq )
		{
			emsi_seq = 0;
			DEB((D_HSHAKE, "rx_init: bad character 0x%02x in \"%s\"",
					c, string_printable(buf)));
		}
	}
	
	return 1;
}
