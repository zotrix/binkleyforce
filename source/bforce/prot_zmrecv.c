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
#include "prot_zmodem.h"

typedef enum zrxstates {
	ZRX_INIT,		/* Send ZRINIT frame -> goto ZRX_INITACK     */
	ZRX_INITACK,		/* Wait for answer on ZRINIT (for ZRQINIT?)  */
	ZRX_GOTFILE,		/* We got ZFINFO -> check it, open, etc..    */
	ZRX_SENDRPOS,		/* Send position we want receive file from   */
	ZRX_RPOSACK,		/* Wait for answer on (ZRPOS) our postion    */
	ZRX_WAITDATA,		/* And wait for data block with file's data  */
	ZRX_REFUSE,		/* Feee... Refuse this file                  */
	ZRX_SKIP		/* Burn baby.. burn.. skip it!               */
} e_zrxstates;

static void zmodem_puts(const char *s);
static int  zmodem_proc_ZFILE(s_protinfo *pi, char *blkptr, size_t blklen);

/* ------------------------------------------------------------------------- */
/* Receive files with Z-Modem protocol                                       */
/* Return:                                                                   */
/*  PRC_SUCCESSFUL: All files received or transfer was finished cleanly      */
/*  PRC_ERROR: Error at sending something(include carrier lost)              */
/*  PRC_ERROR: Error at receiving something(include carrier lost)            */
/*  PRC_MERROR: Misc. errors (i.e. incompatibilies, failures, etc)           */
/*  PRC_TRIESOUT: Very many tries doing something without result             */
/*  PRC_REMOTEDEAD: For a long time no real information transfered           */
/*  PRC_REMOTEABORT: Transfer was aborted by remote (i.e. five CAN, etc..)   */
/*  PRC_LOCALBORT: Transfer was aborted by local (i.e. CPS too low, etc..)   */
/* ------------------------------------------------------------------------- */
int rx_zmodem(s_protinfo *pi, bool caller)
{
	char  zconv;        /* ZMODEM file conversion request              */
	char  zmanag;       /* ZMODEM file management request              */
	char  ztrans;       /* ZMODEM file transport request               */
	char  zexten;       /* ZMODEM file extended options                */
	char *rxbuf;        /* Buffer with ZMAXBLOCKLEN size               */
	int   zrqinitcnt;   /* Count received ZRQINITs                     */
	int   zfincnt;      /* Count received ZFINs (in state ZTX_INITACK) */
	int   inithdrtype;  /* Send this header at ZRX_INIT state          */
	int   txtries = 0;
	int   skipbypos = 0;
	int   ftype = 0;
	enum zrxstates rxstate;
	time_t deadtimer;
	int   rc = PRC_NOERROR;
	int   n, c;

	log("start %s receive", Protocols[state.handshake->protocol]);
	DEB((D_PROT, "start %s receive", Protocols[state.handshake->protocol]));
	
	if( pi->start_time == 0 )
		pi->start_time = time(NULL);
	
	skipbypos      = conf_boolean(cf_zmodem_skip_by_pos);
	rxbuf          = (char *)xmalloc(ZMAXBLOCKLEN+1);
	zrqinitcnt     = 0;
	zfincnt        = 0;
	inithdrtype    = ZRINIT;
	Z_Rxwait       = ZWAITTIME;
	Z_Rxtout       = ZRXTIMEOUT;
	rxstate        = ZRX_INIT;
	setalarm(Z_Rxtout);

	timer_set(&deadtimer, ZDEADTIMER);

	while(1)
	{
                DEB((D_PROT, "rx_zmodem: one more loop"));
		if( timer_expired(deadtimer) )
		{
			DEB((D_PROT, "rx_zmodem: deadtimer = %ld", (long)deadtimer));
			DEB((D_PROT, "rx_zmodem: rxstate = %d", (int)rxstate));
			DEB((D_PROT, "rx_zmodem: rc = %d", rc));
			DEB((D_PROT, "rx_zmodem: txtries = %d", txtries));
			
			log("brain dead! (abort)");
			
			gotoexit(PRC_LOCALABORTED);
		}
		
		if( rxstate == ZRX_INIT || rxstate == ZRX_SENDRPOS )
		{
			if( ++txtries > ZMAXTRIES )
			{
				log("tries reached miximal count");
				gotoexit(PRC_LOCALABORTED);
			}
		}
		
		switch(rxstate) {
		case ZRX_INIT:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_INIT"));
			
			stohdr(Z_Txhdr, 0L);
			
			Z_Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO|CANBRK;
			
			if( Z_Ctlesc )
				Z_Txhdr[ZF0] |= TESCCTL;
			
			if( zshhdr(inithdrtype, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			
			if( inithdrtype == ZSKIP )
				inithdrtype = ZRINIT;
			
			setalarm(Z_Rxtout);
			rxstate = ZRX_INITACK;
			break;
			
		case ZRX_GOTFILE:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_GOTFILE"));
			
			if( pi->recv && pi->recv->fp )
				p_rx_fclose(pi);
			
			switch(zmodem_proc_ZFILE(pi, rxbuf, Z_Rxcount)) {
			case 0:
				txtries = 0;
				rxstate = ZRX_SENDRPOS;
				break;
			case 2:
				rxstate = ZRX_SKIP;
				break;
			default:
				rxstate = ZRX_REFUSE;
				break;
			}
			break;
			
		case ZRX_SENDRPOS:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_SENDRPOS"));
			
			stohdr(Z_Txhdr, pi->recv->bytes_received);
			
			if( zshhdr(ZRPOS, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			
			setalarm(Z_Rxtout);
			rxstate = ZRX_RPOSACK;
			break;
			
		case ZRX_WAITDATA:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_WAITDATA"));
			
			if( (rc = p_info(pi, 0)) )
				gotoexit(rc);
			
			setalarm(Z_Rxtout);
			
			switch(c = zrdata(rxbuf, ZMAXBLOCKLEN, pi->recv->bytes_received)) {
			case ZHANGUP:
			case ZEXIT:
				gotoexit(PRC_ERROR);
				break;
			case ZCAN:
				gotoexit(PRC_REMOTEABORTED);
				break;
			case ZTIMER:
				log("time out waiting for data");
				txtries = 0;
				rxstate = ZRX_SENDRPOS;
				break;
			case ZERROR:
			case ZCRCERR:	/* CRC error */
				zmodem_puts(Z_Attn);
				txtries = 0;
				rxstate = ZRX_SENDRPOS;
				break;
			case GOTCRCW:
			case GOTCRCQ:
			case GOTCRCG:
			case GOTCRCE:
				timer_set(&deadtimer, ZDEADTIMER);
				
				if( (n = p_rx_writefile(rxbuf, Z_Rxcount, pi)) < 0 )
				{
					rxstate = ( n == -2 ) ? ZRX_SKIP : ZRX_REFUSE;
					break;
				}
				
				pi->recv->bytes_received += Z_Rxcount;
				
				switch(c) {
				case GOTCRCW:
					stohdr(Z_Txhdr, pi->recv->bytes_received);
					if( zshhdr(ZACK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
					rxstate = ZRX_RPOSACK;
					break;
				case GOTCRCQ:
					stohdr(Z_Txhdr, pi->recv->bytes_received);
					if( zshhdr(ZACK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
					break;
				case GOTCRCG:
					break;
				case GOTCRCE:
					txtries = 0;
					rxstate = ZRX_RPOSACK;
					break;
				}
				break;
			}
			break;
			
		case ZRX_REFUSE:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_REFUSE"));
			txtries = 0;
			inithdrtype = ZFERR;
			rxstate = ZRX_INIT;
			break;
			
		case ZRX_SKIP:
			DEB((D_PROT, "rx_zmodem: entering state ZRX_SKIP"));
			txtries = 0;
			if( skipbypos )
			{
				pi->recv->bytes_received = pi->recv->bytes_total;
				rxstate = ZRX_SENDRPOS;
			}
			else
			{
				inithdrtype = ZSKIP;
				rxstate = ZRX_INIT;
			}
			break;
			
		default:
			break;
		} /* end of switch(rxstate) */
		
		
		if( rxstate != ZRX_INIT     && rxstate != ZRX_GOTFILE
		 && rxstate != ZRX_SENDRPOS && rxstate != ZRX_WAITDATA
		 && rxstate != ZRX_SKIP     && rxstate != ZRX_REFUSE )
		{
			switch( ftype = zgethdr(Z_Rxhdr) ) {
			case ZCAN:
				gotoexit(PRC_REMOTEABORTED);
				break;
				
			case ZHANGUP:
			case ZEXIT:
				gotoexit(PRC_ERROR);
				break;
			
			case ZTIMER:
				log("time out");
				switch(rxstate) {
				case ZRX_INITACK: rxstate = ZRX_INIT;     break;
				case ZRX_RPOSACK: rxstate = ZRX_SENDRPOS; break;
				default:	break;
				}
				break;
				
			case ZERROR:
			case ZCRCERR:
				/* NAK them all! (TODO: think a little) */
				stohdr(Z_Txhdr, 0L);
				
				if( zshhdr(ZNAK, Z_Txhdr) < 0 )
					gotoexit(PRC_ERROR);
				
				break;
				
			case ZRQINIT:
				if( rxstate == ZRX_INITACK )
				{
					zrqinitcnt++;
					rxstate = ZRX_INIT;
				}
				break;
				
			case ZFILE:
				if( rxstate == ZRX_INITACK
				 || rxstate == ZRX_RPOSACK )
				{
					zconv  = Z_Rxhdr[ZF0];
					zmanag = Z_Rxhdr[ZF1];
					ztrans = Z_Rxhdr[ZF2];
					zexten = Z_Rxhdr[ZF3];
			
					/* default to "binary" mode */
					if( !zconv )
						zconv = ZCBIN;
			
					inithdrtype = ZRINIT;
					setalarm(Z_Rxtout);
					
					switch(zrdata(rxbuf, ZMAXBLOCKLEN, 0)) {
					case ZHANGUP:
					case ZEXIT:
						gotoexit(PRC_ERROR);
						break;
					case ZTIMER:
						txtries = 0;
						rxstate = ZRX_INIT;
						break;
					case GOTCRCW:
						rxstate = ZRX_GOTFILE;
						break;
					default:
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZNAK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						rxstate = ZRX_INITACK;
						break;
					}
				}
				break;
				
			case ZSINIT:
				if( rxstate == ZRX_INITACK )
				{
					Z_Ctlesc  = TESCCTL & Z_Rxhdr[ZF0];
					setalarm(Z_Rxtout);
					switch( zrdata(Z_Attn, ZATTNLEN, 0) ) {
					case ZHANGUP:
					case ZEXIT:
						gotoexit(PRC_ERROR);
						break;
					case ZTIMER:
						txtries = 0;
						rxstate = ZRX_INIT;
						break;
					case GOTCRCW:
						stohdr(Z_Txhdr, 1L);
						if( zshhdr(ZACK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						rxstate = ZRX_INIT;
						break;
					default:
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZNAK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						break;
					}
				}
				break;

			case ZNAK:
				switch(rxstate) {
				case ZRX_INITACK: rxstate = ZRX_INIT;      break;
				case ZRX_RPOSACK: rxstate = ZRX_SENDRPOS;  break;
				default:          break;
				}
				break;
				
			case ZEOF:
				if( rxstate == ZRX_RPOSACK )
				{
					if( rclhdr(Z_Rxhdr) == pi->recv->bytes_received )
					{
						pi->recv->status = FSTAT_SUCCESS;
						
						if( !p_rx_fclose(pi) )
						{
							rxstate = ZRX_INIT;
							inithdrtype = ZRINIT;
						} else
							rxstate = ZRX_REFUSE;
						
						txtries = 0;
					} else
						log("out of sync");
				}
				break;
			
			case ZSKIP:
				if( rxstate == ZRX_RPOSACK )
				{
					log("remote side skipped file");
					rxstate = ZRX_INIT;
				}
				break;
				
			case ZDATA:
				if( rxstate == ZRX_RPOSACK )
				{
					if( rclhdr(Z_Rxhdr) == pi->recv->bytes_received )
					{
						rxstate = ZRX_WAITDATA;
					}
					else
					{
						log("out of sync");
						zmodem_puts(Z_Attn);
						txtries = 0;
						rxstate = ZRX_SENDRPOS;
					}
				}
				break;
				
			case ZFREECNT:
				if( rxstate == ZRX_INITACK )
				{
					stohdr(Z_Txhdr, ~0L);
					if( zshhdr(ZACK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
				}
				break;
				
			case ZCOMMAND:
				if( rxstate == ZRX_INITACK )
				{
					inithdrtype = ZRINIT;
					setalarm(Z_Rxtout);
					
					switch( zrdata(rxbuf, ZMAXBLOCKLEN, 0) ) {
					case ZHANGUP:
					case ZEXIT:
						gotoexit(PRC_ERROR);
						break;
					case ZTIMER:
						txtries = 0;
						rxstate = ZRX_INIT;
						break;
					case GOTCRCW:
						log("remote command execution requested (n/a)");
						log("command: \"%s\"", rxbuf);
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZCOMPL, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						gotoexit(PRC_LOCALABORTED);
						break;
					default:
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZNAK, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						txtries = 0;
						rxstate = ZRX_INIT;
						break;
					}
				}
				break;
				
			case ZFIN:
				if( zrqinitcnt )
				{
					stohdr(Z_Txhdr, 0L);
					if( zshhdr(ZFIN, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
					gotoexit(PRC_NOERROR);
				}
				else if( rxstate == ZRX_INITACK )
				{
					/*
					 * Don't believe first ZFIN on outgoing calls
					 */
					if( ++zfincnt > ZRXSKIPFIN || !caller )
					{
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZFIN, Z_Txhdr) < 0 ) gotoexit(PRC_ERROR);
						gotoexit(PRC_NOERROR);
					}
					rxstate = ZRX_INIT;
				}
				break;
				
			default:
				log("got unexpected frame %d", ftype);
				break;
			}
		}
	} /* end of while(1) */

exit:
	DEB((D_PROT, "rx_zmodem: RECV exit = %d", rc));
	
	setalarm(0);
	
	if( pi->recv && pi->recv->fp ) 
		p_rx_fclose(pi);
	
	if( rxbuf )
		free(rxbuf);
	
	return(rc);
}

/* ------------------------------------------------------------------------- */
/* Send a string to the modem, processing for \336 (sleep 1 sec)             */
/* and \335 (break signal)                                                   */
/* ------------------------------------------------------------------------- */
static void zmodem_puts(const char *s)
{
	DEB((D_PROT, "zmodem_puts: \"%s\"", string_printable(s)));
	
	while( s && *s )
	{
		char *p = strpbrk(s, "\335\336");
		
		if( !p )
		{
			WRITE_TIMEOUT(s, strlen(s));
			return;
		}
		if( p != s )
		{
			WRITE_TIMEOUT(s, p-s);
			s = p;
		}
		
		if( *p == '\336' )
			sleep(1);
		else
			tio_send_break();
		
		p++;
	}
}

/* ------------------------------------------------------------------------- */
/* Process incoming file information header                                  */
/* ------------------------------------------------------------------------- */
static int zmodem_proc_ZFILE(s_protinfo *pi, char *blkptr, size_t blklen)
{
	char  *fileiptr = NULL;
	size_t filesize = 0L;
	time_t filetime = 0L;

#ifdef DEBUG
	char *tmp = string_printable_buffer(blkptr, blklen);

	DEB((D_PROT, "zmodem_proc_ZFILE: process \"%s\" (%ld bytes)",
			tmp, blklen));

	if( tmp )
	{
		free(tmp); tmp = NULL;
	}
#endif

	blkptr[blklen] = '\0';
	
	fileiptr = blkptr + strlen(blkptr) + 1;
	
	if( fileiptr >= (blkptr + blklen) ||
	    sscanf(fileiptr, "%d%lo", &filesize, (unsigned long *)&filetime) < 1 )
	{
		log("zmodem: got invalid ZFILE packet");
		return 1;
	}
	
	DEB((D_PROT, "zmodem_proc_ZFILE: filesize=%ld, filetime=%lo",
		(long)filesize, (long)filetime));
	
	return p_rx_fopen(pi, blkptr, filesize, filetime, 0);
}

