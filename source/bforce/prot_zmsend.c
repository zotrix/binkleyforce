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
#include "prot_common.h"
#include "prot_zmodem.h"

/* ------------------------------------------------------------------------- */
/* States of our Zmodem sending protocol                                     */
/* ------------------------------------------------------------------------- */
typedef enum ztxstates {
	ZTX_START,		/* Start sending (init, send "rz\r")         */
	ZTX_RQINIT,		/* Send ZRQINIT frame                        */
	ZTX_RQINITACK,		/* Wait for answer on ZRQINIT frame          */
	ZTX_NEXTFILE,		/* Prepare next file for sending             */
	ZTX_FINFO,		/* Send file's ZFINFO frame with data        */
	ZTX_FINFOACK,		/* Wait for answer on our ZFINFO + ..        */
	ZTX_STARTDATA,		/* Start sending of data (send ZDATA?)       */
	ZTX_DATA,		/* Send new portion of data blocks           */
	ZTX_READCHECK,		/* Check - we got some chars                 */
	ZTX_CRCWACK,		/* Wait for CRCW ack                         */
	ZTX_CRCQACK,		/* Wait for CRCQ ack                         */
	ZTX_EOF,		/* Send ZEOF frame with our file length      */
	ZTX_EOFACK,		/* Wait for answer on our ZEOF               */
	ZTX_FIN,		/* Send ZFIN to finish sending               */
	ZTX_FINACK		/* Wait for answer on sent ZFIN              */
} e_ztxstates;

static void zmodem_add_empty_packet(s_protinfo *pi)
{
	struct stat st;
	s_filelist **ptrl;
	s_packet pkt;
	char tmpname[] = "/tmp/bfXXXXXX";
	char *p_tmpname;
			
	if( (p_tmpname = mktemp(tmpname)) == NULL )
	{
		logerr("cannot generate temp. file name for packet from \"%s\"", tmpname);
		return;
	}
	
	memset(&pkt, '\0', sizeof(s_packet));
	
	pkt.dest = state.node.addr;
	pkt.orig = *state.handshake->remote_address(state.handshake);
	
	if( pkt_createpacket(p_tmpname, &pkt) )
	{
		char abuf[BF_MAXADDRSTR+1];
		logerr("cannot create packet for address %s",
			ftn_addrstr(abuf, pkt.dest));
		return;
	}
	
	if( stat(p_tmpname, &st) )
	{
		logerr("cannot stat created packet \"%s\"", p_tmpname);
		unlink(p_tmpname);
		return;
	}
	
	for( ptrl = &pi->filelist; *ptrl; ptrl = &((*ptrl)->next) )
		{ /* EMPTY LOOP */ }
	
	(*ptrl) = (s_filelist *)xmalloc(sizeof(s_filelist));
	memset(*ptrl, '\0', sizeof(s_filelist));
	(*ptrl)->fname  = xstrcpy(p_tmpname);
	(*ptrl)->type   = TYPE_NETMAIL;
	(*ptrl)->action = ACTION_FORCEUNLINK;
	(*ptrl)->size   = st.st_size;
}

/* ------------------------------------------------------------------------- */
/* Send files with Z-Modem protocol                                          */
/* Files to transfer stored in pi->syslist structure                         */
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
int tx_zmodem(s_protinfo *pi, bool caller)
{
	int   startblk  = 64;  /* Initial Zmodem block size                  */
	int   minblk    = 64;  /* Minimal Z-protocol block size              */
	int   maxblk    = 1024;/* Maximal Z-protocol block size              */
	int   blocklen  = 0;   /* Length of transmitted blocks               */
	int   goodblk   = 0;   /* How many blocks we sent w/o ZRPOS'tion :)  */
	int   txwindow  = 0;   /* Tranmitter window size (0 means streaming) */
	int   newcnt    = 0;   /* Count free bytes in receiver's buffer      */
	int   rxbuflen  = 0;   /* Receiver's max buffer length               */
	int   rxlastpos = 0;   /* Receiver's last reported offset            */
	int   beenhere  = 0;   /* How many times we've been ZRPOS'd same place */
	long  bytescnt  = 0;   /* Received bytes(current offset)             */
	long  lastsync  = 0;   /* Last offset to which we got a ZRPOS        */
	char  zconv     = 0;   /* Local ZMODEM file conversion request       */
	char  zmanag    = 0;   /* Local ZMODEM file management request       */
	char  ztrans    = 0;   /* Local ZMODEM file translation request      */
	char  zexten    = 0;   /* Local ZMODEM file extended options         */
	char *txbuf     = NULL;/* Buffer with ZMAXBLOCKLEN size              */
	int   zrinitcnt = 0;   /* Count received ZRINITs                     */
	int   rxflags1  = 0;
	int   rxflags2  = 0;
	int   txtries   = 0;
	int   junkcnt   = 0;
	int   initacked = 0;   /* TRUE when at least one ZRQINIT was sent    */
	                       /* after first ZRINIT was received            */
	int   rc        = 0;   /* Our return code                            */
	int   dtype, n;
	int   ftype;
	char  c, *p;
	long unsigned crc32;
	enum  ztxstates txstate;
	time_t deadtimer;
	
	log("start %s send", Protocols[state.handshake->protocol]);
	DEB((D_PROT, "start %s send", Protocols[state.handshake->protocol]));
	
	/* Set time transfer started at */
	if( pi->start_time == 0 )
		pi->start_time = time(NULL);
	
	txbuf      = (char *)xmalloc(ZMAXBLOCKLEN+1);
	zconv      = ZCBIN;
	maxblk     = (state.handshake->protocol == PROT_ZMODEM) ? 1024 : 8192;
	
	/* Set initial block size (default is 128b) */
	if( (startblk = conf_number(cf_zmodem_start_block_size)) > 0 )
	{
		if( startblk%64 || startblk > maxblk || startblk < 64 )
			startblk = 256;
	} else 
		startblk = 256;
	
	blocklen  = startblk;
	txwindow  = conf_number(cf_zmodem_tx_window);
	Z_Rxwait  = ZWAITTIME;
	Z_Rxtout  = ZRXTIMEOUT;
	txstate   = ZTX_START;

	timer_set(&deadtimer, ZDEADTIMER);
	
	setalarm(Z_Rxtout);
	
	/*
	 * At zmodem batches send empty netmail packet
	 * if no real outgoing traffic available
	 */
	if( !pi->send_left_size && conf_boolean(cf_zmodem_send_dummy_pkt) )
		zmodem_add_empty_packet(pi);
	
	while(1)
	{
		if( timer_expired(deadtimer) )
		{
			log("brain dead! (abort)");
			gotoexit(PRC_LOCALABORTED);
		}
		
		if( txstate == ZTX_RQINIT || txstate == ZTX_FINFO
		 || txstate == ZTX_EOF    || txstate == ZTX_FIN )
		{
#ifdef DEBUG
			if( txtries ) DEB((D_PROT, "tx_zmodem: try #%d", txtries));
#endif			
			if( ++txtries > ZMAXTRIES )
			{
				log("out of tries");
				gotoexit(PRC_LOCALABORTED);
			}
		}
		
		switch(txstate) {
		case ZTX_START:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_START"));
			if( PUTSTR("rz\r") < 0 )
				gotoexit(PRC_ERROR);
			txtries = 0;
			txstate = ZTX_RQINIT;
			break;
			
		case ZTX_RQINIT:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_RQINIT"));
			stohdr(Z_Txhdr, 0L);
			if( zshhdr(ZRQINIT, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			setalarm(Z_Rxtout);
			txstate = ZTX_RQINITACK;
			break;
			
		case ZTX_NEXTFILE:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_NEXTFILE"));
			if( pi->send && pi->send->fp )
				p_tx_fclose(pi);
			txtries = 0;
			txstate = p_tx_fopen(pi) ? ZTX_FIN : ZTX_FINFO;
			break;

		case ZTX_FINFO:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_FINFO"));
			
			zrinitcnt = 0;
			
			strnxcpy(txbuf, pi->send->net_name, ZMAXFNAME);
			
			p = txbuf + strlen(txbuf) + 1; 
			sprintf(p, "%ld %lo %lo 0 %ld %ld",
				(long)pi->send->bytes_total, (long)pi->send->mod_time,
				(long)pi->send->mode, (long)pi->send_left_num,
				(long)pi->send_left_size);
			
			DEB((D_PROT, "tx_zmodem: send \"%s\\000%s\"", txbuf, p));
			
			Z_Txhdr[ZF0] = zconv;	/* file conversion request */
			Z_Txhdr[ZF1] = zmanag;  /* file management request */
			Z_Txhdr[ZF2] = ztrans;  /* file transport request  */
			Z_Txhdr[ZF3] = zexten;
			
			if( zsbhdr(ZFILE, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			if( zsdata(txbuf, (p - txbuf) + strlen(p), ZCRCW, 0) < 0 )
				gotoexit(PRC_ERROR);
			
			setalarm(Z_Rxtout);
			txstate = ZTX_FINFOACK;
			break;
			
		case ZTX_STARTDATA:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_STARTDATA"));
			
			newcnt   = rxbuflen;
			junkcnt  = 0;
			
			stohdr(Z_Txhdr, pi->send->bytes_sent);
			if( zsbhdr(ZDATA, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			
			txstate = ZTX_DATA;
			break;
			
		case ZTX_DATA:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_DATA"));
			
			timer_set(&deadtimer, ZDEADTIMER);
			setalarm(Z_Rxtout); /* Remove annoing timeouts! */
			
			if( (n = p_tx_readfile(txbuf, blocklen, pi)) < 0 )
			{
				/* error occured, remote wait for DATA */
				/* so send null ZCRCE data subpacket   */
				if( zsdata(txbuf, 0, ZCRCE, 0) < 0 )
					gotoexit(PRC_ERROR);
				txstate = ZTX_NEXTFILE;
				break;
			}
		
			if( pi->send->eofseen )
				dtype = ZCRCE;
			else if( junkcnt > 6 )
				dtype = ZCRCW;
			else if( bytescnt == lastsync )
				dtype = ZCRCW;
			else if( rxbuflen && (newcnt -= n) <= 0 )
				dtype = ZCRCW;
			else if( txwindow && (bytescnt - rxlastpos + n) >= txwindow )
				dtype = ZCRCQ;
			else
				dtype = ZCRCG;

			if( (rc = p_info(pi, 0)) )
				gotoexit(rc);
			
			if( zsdata(txbuf, n, dtype, pi->send->bytes_sent) < 0 )
				gotoexit(PRC_ERROR);
			
			if( ++goodblk > 5 && blocklen*2 <= maxblk )
			{
				goodblk = 0;
				blocklen *= 2;
				DEB((D_PROT, "tx_zmodem: new blocklen = %ld byte(s)", blocklen));
			}
			
			bytescnt = pi->send->bytes_sent += n;
			
			if( dtype == ZCRCW )
			{
				junkcnt = 0;
				setalarm(Z_Rxtout);
				txstate = ZTX_CRCWACK;
				break;
			}
			else if( dtype == ZCRCQ )
			{
				junkcnt = 0;
				setalarm(Z_Rxtout);
				txstate = ZTX_CRCQACK;
				break;
			}
			else if( dtype == ZCRCE )
			{
				txtries = 0;
				txstate = ZTX_EOF;
				break;
			}
			
			if( CHARWAIT(0) )
			{
				while( (rc = GETCHAR(1)) != ZTIMER )
				{
					if( rc < 0 )
					{
						gotoexit(PRC_ERROR);
					}
					else if( rc == CAN || rc == ZPAD )
					{
						DEB((D_PROT, "tx_zmodem: got ZPAD or CAN!"));
						setalarm(Z_Rxtout);
						txstate = ZTX_READCHECK;
						break;
					}
					else if( rc == XOFF || rc == (XOFF|0200) )
					{
						DEB((D_PROT, "tx_zmodem: got XOFF"));
						if( GETCHAR(5) < 0 )
							gotoexit(PRC_ERROR);
						break;
					}
					else if( rc == XON  || rc == (XON|0200) )
					{
						DEB((D_PROT, "tx_zmodem: got XON"));
					}
					else
					{
						junkcnt++;
						DEB((D_PROT, "tx_zmodem: got JUNK = 0x%x (junkcnt = %d)",
							rc, junkcnt));
					}
				} /* end of while( rc != ZTIMER ) */
			} /* end of if( CHARWAIT(0) ) */
			break;
		
		case ZTX_EOF:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_EOF"));
			
			stohdr(Z_Txhdr, pi->send->bytes_sent);
			if( zsbhdr(ZEOF, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			
			setalarm(Z_Rxtout);
			txstate = ZTX_EOFACK;
			break;
			
		case ZTX_FIN:
			DEB((D_PROT, "tx_zmodem: entering state ZTX_FIN"));
			
			stohdr(Z_Txhdr, 0L);
			if( zshhdr(ZFIN, Z_Txhdr) < 0 )
				gotoexit(PRC_ERROR);
			
			setalarm(Z_Rxtout);
			txstate = ZTX_FINACK;
			break;
			
		default:
			/* Ignore them all */
			break;
		} /* end of switch(txstate) */
	
		if( txstate != ZTX_START  && txstate != ZTX_RQINIT
		 && txstate != ZTX_FINFO  && txstate != ZTX_DATA
		 && txstate != ZTX_EOF    && txstate != ZTX_FIN )
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
				
				if( txstate == ZTX_READCHECK )
					zsdata(txbuf, 0, ZCRCE, 0);
				
				switch(txstate) {
				case ZTX_RQINITACK: txstate = ZTX_RQINIT;    break;
				case ZTX_FINFOACK:  txstate = ZTX_FINFO;     break;
				case ZTX_READCHECK: txstate = ZTX_STARTDATA; break;
				case ZTX_CRCWACK:   txstate = ZTX_STARTDATA; break;
				case ZTX_CRCQACK:   txstate = ZTX_STARTDATA; break;
				case ZTX_EOFACK:    txstate = ZTX_EOF;       break;
				case ZTX_FINACK:    txstate = ZTX_FIN;       break;
				default:            break;
				}
				break;
				
			case ZERROR:
			case ZCRCERR:
				/* NAK them all! */
				stohdr(Z_Txhdr, 0L);
				if( zshhdr(ZNAK, Z_Txhdr) < 0 )
					gotoexit(PRC_ERROR);
				break;
				
			case ZRQINIT:
				if( txstate == ZTX_RQINITACK )
				{
					if( Z_Rxhdr[0] == ZCOMMAND )
						break;
					
					stohdr(Z_Txhdr, 0L);
					if( zshhdr(ZNAK, Z_Txhdr) < 0 )
						gotoexit(PRC_ERROR);
					
					txstate = ZTX_RQINIT;
				}
				else if( txstate == ZTX_FINFOACK )
				{
					/* remote is sender - abort */
					log("zmodem: remote is sender");
					gotoexit(PRC_LOCALABORTED);
				}
				break;
				
			case ZRINIT:
				if( txstate == ZTX_RQINITACK )
				{
					if( initacked == 0 )
					{
						/* Be sure ack first ZRINIT */
						stohdr(Z_Txhdr, 0L);
						if( zshhdr(ZRQINIT, Z_Txhdr) < 0 )
							gotoexit(PRC_ERROR);
						initacked = 1;
					}
					
					/* Get receiver's options */
					rxflags1  = (0377 & Z_Rxhdr[ZF0]);
					rxflags2  = (0377 & Z_Rxhdr[ZF1]);
					Z_Txfcs32 = (rxflags1 & CANFC32);
					Z_Ctlesc |= (rxflags1 & TESCCTL);
					rxbuflen  = (0377 & Z_Rxhdr[ZP0]);
					rxbuflen += ((0377 & Z_Rxhdr[ZP1])<<8);
					
					/* No ZCRCQ if remote doesn't indicate */
					/* FDX ability                         */ 
					if( !(rxflags1 & CANFDX) )
						txwindow = 0;
				
					DEB((D_PROT, "tx_zmodem: Z_Txfcs32 = %d Z_Ctlesc = %d",
						Z_Txfcs32, Z_Ctlesc));
					DEB((D_PROT, "tx_zmodem: rxbuflen = %d blocklen = %d",
						rxbuflen, blocklen));
					DEB((D_PROT, "tx_zmodem: txwindow = %u",
						txwindow));
				
					txstate   = ZTX_NEXTFILE;
				}
				else if( txstate == ZTX_FINFOACK )
				{
					/* Possible they didn't see */
					/* our file information     */
					if( ++zrinitcnt > 2 )
						txstate = ZTX_FINFO;
				}
				else if( txstate == ZTX_READCHECK
				      || txstate == ZTX_CRCQACK
				      || txstate == ZTX_CRCWACK )
				{
					if( txstate == ZTX_READCHECK
					 || txstate == ZTX_CRCQACK )
						zsdata(txbuf, 0, ZCRCE, 0);
					
					/* Assume file normaly sent ? */
					log("assume file normaly sent");
					
					pi->send->status = FSTAT_SUCCESS;
					txstate = ZTX_NEXTFILE;
				}
				else if( txstate == ZTX_EOFACK )
				{
					/* ok, send next */
					pi->send->status = FSTAT_SUCCESS;
					txstate = ZTX_NEXTFILE;
				}
				else if( txstate == ZTX_FINACK )
				{
					/* Possible we should ignore  */
					/* first ZRINIT. Because they */
					/* didn't see our first ZFIN  */
					/* But I'm soo lazy .. :))    */
					txstate = ZTX_FIN;
				}
				break;
				
			case ZACK:
				if( txstate == ZTX_CRCWACK )
				{
					rxlastpos = Z_Rxpos;
					if( pi->send->bytes_sent == Z_Rxpos )
						txstate = ZTX_STARTDATA;
				}
				else if( txstate == ZTX_READCHECK
				      || txstate == ZTX_CRCQACK )
				{
					rxlastpos = Z_Rxpos;
					txstate   = ZTX_DATA;
				}
				break;
				
			case ZSKIP:
				if( txstate == ZTX_FINFOACK
				 || txstate == ZTX_READCHECK
				 || txstate == ZTX_CRCQACK
				 || txstate == ZTX_CRCWACK
				 || txstate == ZTX_EOFACK )
				{
					if( txstate == ZTX_READCHECK
					 || txstate == ZTX_CRCQACK )
						zsdata(txbuf, 0, ZCRCE, 0);
					
					if( txstate == ZTX_READCHECK )
						CLEAROUT();
					
					pi->send->status = FSTAT_SKIPPED;
					log("remote side skipped file");
					
					txstate = ZTX_NEXTFILE;
				}
				break;
				
			case ZFIN:
				/* BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG! */
				/* BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG! */
				/* BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG!BUG! */
				if( txstate == ZTX_FINACK )
				{
					if( PUTSTR("OO") == 0 )
						FLUSHOUT();
					gotoexit(PRC_NOERROR);
				}
				break;
				
			case ZRPOS:
				if( txstate == ZTX_FINFOACK
				 || txstate == ZTX_READCHECK
				 || txstate == ZTX_CRCQACK
				 || txstate == ZTX_CRCWACK
				 || txstate == ZTX_EOFACK )
				{
					rxlastpos = Z_Rxpos;
					
					/* Clear modem buffers */
					/* if( txstate != FINFOACK ) SENDBREAK(); */
					if( txstate == ZTX_READCHECK ) CLEAROUT();

					if( txstate == ZTX_READCHECK
					 || txstate == ZTX_CRCQACK )
					{
						if( zsdata(txbuf, 0, ZCRCE, 0) < 0 )
							gotoexit(PRC_ERROR);
					}
					
					/* Reset EOF flag! */
					pi->send->eofseen = FALSE;
					
					/* Check pos */
					if( (Z_Rxpos || txstate != ZTX_FINFOACK)
					 && fseek(pi->send->fp, Z_Rxpos, 0) )
					{
						logerr("can't send file from requested position");
						/* Open next file for send */
						txstate = ZTX_NEXTFILE;
						break;
					}
					
					if( txstate == ZTX_FINFOACK )
					{
						if( Z_Rxpos )
						{
							log("resyncing at offset %d", Z_Rxpos);
							pi->send->bytes_skipped = Z_Rxpos;
						}
					}
					else if( txstate == ZTX_READCHECK
					      || txstate == ZTX_CRCWACK
					      || txstate == ZTX_CRCQACK )
					{
						goodblk = 0;
						if( lastsync >= Z_Rxpos && ++beenhere > 4 )
							if( blocklen > minblk )
							{
								blocklen /= 2;
								DEB((D_PROT, "tx_zmodem: falldown to %ld BlockLen", blocklen));
							}
					}
					
					lastsync = bytescnt = pi->send->bytes_sent = Z_Rxpos;
					
					if( txstate == ZTX_FINFOACK )
						--lastsync;
					
					txstate = ZTX_STARTDATA;
				}
				break;
				
			case ZNAK:
				switch(txstate) {
				case ZTX_RQINITACK: txstate = ZTX_RQINIT; break;
				case ZTX_FINFOACK:  txstate = ZTX_FINFO;  break;
				case ZTX_EOFACK:    txstate = ZTX_EOF;    break;
				case ZTX_FINACK:    txstate = ZTX_FIN;    break;
				default:            break;
				}
				break;
				
			case ZCRC:
				if( txstate == ZTX_FINFOACK )
				{
					/* Send file's CRC-32 */
					crc32 = 0xFFFFFFFFL;
					
					while( ((c = getc(pi->send->fp)) != EOF) && --Z_Rxpos )
						crc32 = updcrc32(c, crc32);
					
					crc32 = ~crc32;
					
					clearerr(pi->send->fp); /* Clear EOF */
					fseek(pi->send->fp, 0L, 0);
					
					stohdr(Z_Txhdr, crc32);
					if( zsbhdr(ZCRC, Z_Txhdr) < 0 )
						gotoexit(PRC_ERROR);
				}
				break;
				
			case ZCHALLENGE:
				if( txstate == ZTX_RQINITACK )
				{
					/* Echo receiver's challenge number */
					stohdr(Z_Txhdr, Z_Rxpos);
					if( zshhdr(ZACK, Z_Txhdr) < 0 )
						gotoexit(PRC_ERROR);
					txstate = ZTX_RQINIT;
				}
				break;
				
			case ZCOMMAND:
				if( txstate == ZTX_RQINITACK )
				{
					txstate = ZTX_RQINIT;
				}
				break;

			case ZABORT:
				log("remote requested for session abort");
				stohdr(Z_Txhdr, 0L);
				if( zshhdr(ZFIN, Z_Txhdr) < 0 )
					gotoexit(PRC_ERROR);
				gotoexit(PRC_REMOTEABORTED);
				break;
				
			case ZFERR:
				if( txstate == ZTX_FINFOACK
				 || txstate == ZTX_READCHECK
				 || txstate == ZTX_CRCWACK
				 || txstate == ZTX_CRCQACK
				 || txstate == ZTX_EOFACK )
				{
					if( txstate == ZTX_READCHECK
					 || txstate == ZTX_CRCQACK )
					{
						if( zsdata(txbuf, 0, ZCRCE, 0) < 0 )
							gotoexit(PRC_ERROR);
					}

					pi->send->status = FSTAT_REFUSED;
					log("remote side refused file");
					
					txstate = ZTX_NEXTFILE;
				}
				break;
				
			default:
				log("got unexpected frame %d", ftype);
				break;
			} /* end of switch(hdr) */
		} /* end of if */
	} /* end of while */
	
exit:
	DEB((D_PROT, "tx_zmodem: SEND exit = %d", rc));
	
	setalarm(0);
	
	if( pi->send && pi->send->fp )
		p_tx_fclose(pi);
	
	if( txbuf )
		free(txbuf);
	
	return(rc);
}

