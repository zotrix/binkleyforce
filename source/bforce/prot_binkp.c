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

typedef enum {
	BPI_SendSysInfo,
	BPI_WaitADR,
	BPI_WaitPWD,
	BPI_Auth
} binkp_incoming_state;

typedef enum {
	BPO_SendSysInfo,
	BPO_WaitNUL,
	BPO_SendPWD,
	BPO_WaitADR,
	BPO_Auth,
	BPO_WaitOK
} binkp_outgoing_state;

#define GOTO(label,newrc)    { rc = (newrc); goto label; }

void binkp_process_NUL(s_binkp_sysinfo *remote_data, char *buffer)
{
	char *p, *q;
	
	if( strncmp(buffer, "SYS ", 4) == 0 )
		strnxcpy(remote_data->systname, buffer+4, sizeof(remote_data->systname));
	else if( strncmp(buffer, "ZYZ ", 4) == 0 )
		strnxcpy(remote_data->sysop, buffer+4, sizeof(remote_data->sysop));
	else if( strncmp(buffer, "LOC ", 4) == 0 )
		strnxcpy(remote_data->location, buffer+4, sizeof(remote_data->location));
	else if( strncmp(buffer, "PHN ", 4) == 0 )
		strnxcpy(remote_data->phone, buffer+4, sizeof(remote_data->phone));
	else if( strncmp(buffer, "NDL ", 4) == 0 )
		strnxcpy(remote_data->flags, buffer+4, sizeof(remote_data->flags));
	else if( strncmp(buffer, "TIME ", 5) == 0 )
		strnxcpy(remote_data->timestr, buffer+5, sizeof(remote_data->timestr));
	else if( strncmp(buffer, "OPT ", 4) == 0 )
	{
		if( *remote_data->opt )
		{
			strnxcat(remote_data->opt, " ", sizeof(remote_data->opt));
			strnxcat(remote_data->opt, buffer+4, sizeof(remote_data->opt));
		}
		else
			strnxcpy(remote_data->opt, buffer+4, sizeof(remote_data->opt));
		
		binkp_parse_options(remote_data, buffer+4);
	}
	else if( strncmp(buffer, "VER ", 4) == 0 )
	{
		/* <mailer> [<protocol>/<vermaj>.<vermin>] */
		if( (p = strchr(buffer+4, ' ')) )
		{
			strnxcpy(remote_data->progname, buffer+4,
					MIN(sizeof(remote_data->progname), p - (buffer+4) + 1));
			++p;
			if( (q = strchr(p, '/')) )
			{
				strnxcpy(remote_data->protname, p,
						MIN(sizeof(remote_data->protname), q - p + 1));
				sscanf(q+1, "%d.%d",
						&remote_data->majorver,
						&remote_data->minorver);
			}
			if ((remote_data->majorver * 100 + 
			     remote_data->minorver)> 100)
			     remote_data->options |= BINKP_OPT_MB;
		}
		else
			strnxcpy(remote_data->progname, buffer+4, sizeof(remote_data->progname));
	}
	else
		log("BinkP got invalid NUL: \"%s\"", string_printable(buffer));
}

void binkp_process_ADR(s_binkp_sysinfo *remote_data, char *buffer)
{
	s_faddr addr;
	char *p, *q;
	
	for( p = string_token(buffer, &q, NULL, 0); p;
	     p = string_token(NULL, &q, NULL, 0) )
	{
		if( ftn_addrparse(&addr, p, FALSE) )
			log("BinkP got unparsable address \"%s\"", string_printable(p));
		else
			session_addrs_add(&remote_data->addrs, &remote_data->anum, addr);
	}
}

int binkp_outgoing(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data)
{
	s_bpinfo bpi;
	binkp_outgoing_state binkp_state = BPO_SendSysInfo;
	int rc = HRC_OK;
	int recv_rc = 0;
	int send_rc = 0;
	bool send_ready = FALSE;
	bool recv_ready = FALSE;
	
	binkp_init_bpinfo(&bpi);
	
	while(1)
	{
		switch(binkp_state) {
		case BPO_SendSysInfo:
			binkp_queue_sysinfo(&bpi, local_data);
			binkp_state = BPO_WaitNUL;
			break;

		case BPO_SendPWD:
			if( *local_data->passwd == '\0' )
			{
				binkp_queuemsg(&bpi, BPMSG_PWD, NULL, "-");
			}
			else if( remote_data->options & BINKP_OPT_MD5 )
			{
				char digest_bin[16];
				char digest_hex[33];
				
				md5_cram_get(local_data->passwd, remote_data->challenge,
						remote_data->challenge_length, digest_bin);
			
				/* Encode digest to the hex string */
				string_bin_to_hex(digest_hex, digest_bin, 16);
				
				binkp_queuemsg(&bpi, BPMSG_PWD, "CRAM-MD5-", digest_hex);
			}
			else
				binkp_queuemsg(&bpi, BPMSG_PWD, NULL, local_data->passwd);

			binkp_state = BPO_WaitADR;
			break;
			
		case BPO_Auth:
			/* Set remote password same as local */
			strncpy(remote_data->passwd, local_data->passwd, BINKP_MAXPASSWD);
			remote_data->passwd[BINKP_MAXPASSWD] = '\0';

			if( !remote_data->anum )
			{
				binkp_queuemsg(&bpi, BPMSG_BSY, NULL, "No addresses was presented");
				GOTO(Exit, HRC_BUSY);
			}
			else if( session_addrs_check_genuine(remote_data->addrs, remote_data->anum,
			                             state.node.addr) )
			{
				binkp_queuemsg(&bpi, BPMSG_ERR, NULL, "Sorry, you are not who I need");
				GOTO(Exit, HRC_NO_ADDRESS);
			}
			else if( session_addrs_check(remote_data->addrs, remote_data->anum,
			                             remote_data->passwd, NULL, 0) )
			{
				binkp_queuemsg(&bpi, BPMSG_ERR, NULL, "Security violation");
				GOTO(Exit, HRC_BAD_PASSWD);
			}
			else if( session_addrs_lock(remote_data->addrs, remote_data->anum) )
			{
				binkp_queuemsg(&bpi, BPMSG_BSY, NULL, "All addresses are busy");
				GOTO(Exit, HRC_BUSY);
			}
			binkp_state = BPO_WaitOK;
			break;

		default:
			break;
		}
		
		/*
		 * Receive/Send next data block
		 */
		send_ready = recv_ready = FALSE;
		
		if( tty_select(&recv_ready, (bpi.opos || bpi.n_msgs) ?
		               &send_ready : NULL, bpi.timeout) < 0 )
			GOTO(Abort, HRC_OTHER_ERR);
		
		recv_rc = BPMSG_NONE;
		send_rc = 0;
		
		if( recv_ready && (recv_rc = binkp_recv(&bpi)) == BPMSG_EXIT )
			GOTO(Abort, HRC_OTHER_ERR);
		
		if( send_ready && (send_rc = binkp_send(&bpi)) < 0 )
			GOTO(Abort, HRC_OTHER_ERR);

		/*
		 * Handle received message
		 */
		switch(recv_rc) {
		case BPMSG_NONE:
			break;

		case BPMSG_NUL:
			binkp_process_NUL(remote_data, bpi.ibuf+1);
			if( binkp_state == BPO_WaitNUL )
				binkp_state = BPO_SendPWD;
			break;
		
		case BPMSG_ADR:
			if( binkp_state == BPO_WaitADR )
			{
				binkp_process_ADR(remote_data, bpi.ibuf+1);
				binkp_state = BPO_Auth;
			}
			break;
		
		case BPMSG_OK:
			if( binkp_state == BPO_WaitOK )
				GOTO(Exit, HRC_OK);
			break;
		
		case BPMSG_ERR:
			log("BinkP error: \"%s\"", string_printable(bpi.ibuf+1));
			GOTO(Abort, HRC_FATAL_ERR);
			
		case BPMSG_BSY:
			log("BinkP busy: \"%s\"", string_printable(bpi.ibuf+1));
			GOTO(Abort, HRC_TEMP_ERR);
		}
	}

Exit:
	if( binkp_flush_queue(&bpi, bpi.timeout) && rc == HRC_OK )
		rc = HRC_OTHER_ERR;

Abort:
	binkp_deinit_bpinfo(&bpi);
	
	return rc;
}


int binkp_auth_incoming(s_binkp_sysinfo *remote_data)
{
	if( remote_data->challenge_length > 0
	 && strncmp(remote_data->passwd, "CRAM-MD5-", 9) == 0 )
	{
		return session_addrs_check(remote_data->addrs,
		                           remote_data->anum,
		                           remote_data->passwd + 9,
		                           remote_data->challenge,
		                           remote_data->challenge_length);
	}
	
	return session_addrs_check(remote_data->addrs, remote_data->anum,
	                           remote_data->passwd, NULL, 0);
}

int binkp_incoming(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data)
{
	s_bpinfo bpi;
	binkp_incoming_state binkp_state = BPI_SendSysInfo;
	int rc = HRC_OK;
	int recv_rc = 0;
	int send_rc = 0;
	bool send_ready = FALSE;
	bool recv_ready = FALSE;
	
	binkp_init_bpinfo(&bpi);
	
	while(1)
	{
		switch(binkp_state) {
		case BPI_SendSysInfo:
			binkp_queue_sysinfo(&bpi, local_data);
			binkp_state = BPI_WaitADR;
			break;
		
		case BPI_Auth:
			/* Set challenge string same as local */
			memcpy(remote_data->challenge, local_data->challenge,
					sizeof(remote_data->challenge));
			remote_data->challenge_length = local_data->challenge_length;
			
			/* Do authorization */
			if( !remote_data->anum )
			{
				binkp_queuemsg(&bpi, BPMSG_BSY, NULL, "No addresses was presented");
				GOTO(Exit, HRC_BUSY);
			}
			else if( binkp_auth_incoming(remote_data) )
			{
				binkp_queuemsg(&bpi, BPMSG_ERR, NULL, "Security violation");
				GOTO(Exit, HRC_BAD_PASSWD);
			}
			else if( session_addrs_lock(remote_data->addrs, remote_data->anum) )
			{
				binkp_queuemsg(&bpi, BPMSG_BSY, NULL, "All addresses are busy");
				GOTO(Exit, HRC_BUSY);
			}
			else
			{
				binkp_queuemsg(&bpi, BPMSG_OK, NULL, NULL);
				GOTO(Exit, HRC_OK);
			}
			break;

		default:
			break;
		}
		
		/*
		 * Receive/Send next data block
		 */
		send_ready = recv_ready = FALSE;
		
		if( tty_select(&recv_ready, (bpi.opos || bpi.n_msgs) ?
		               &send_ready : NULL, bpi.timeout) < 0 )
			GOTO(Abort, HRC_OTHER_ERR);
		
		recv_rc = BPMSG_NONE;
		send_rc = 0;
		
		if( recv_ready && (recv_rc = binkp_recv(&bpi)) == BPMSG_EXIT )
			GOTO(Abort, HRC_OTHER_ERR);
		
		if( send_ready && (send_rc = binkp_send(&bpi)) < 0 )
			GOTO(Abort, HRC_OTHER_ERR);

		/*
		 * Handle received message
		 */
		switch(recv_rc) {
		case BPMSG_NONE:
			break;

		case BPMSG_NUL:
			binkp_process_NUL(remote_data, bpi.ibuf+1);
			break;
		
		case BPMSG_ADR:
			if( binkp_state == BPI_WaitADR )
			{
				int i;
				char *szOpt = xstrcpy (" MB");
				s_override ovr;
				binkp_process_ADR(remote_data, bpi.ibuf+1);
				for(i = 0; i < remote_data->anum; i++)
				{
					ovr.sFlags = "";
					override_get (&ovr, remote_data->addrs[i].addr, 0);
					if (!nodelist_checkflag (ovr.sFlags, "NR"))
					{
						szOpt = xstrcat (szOpt, " NR");
						break;
					}
				}
				binkp_queuemsg(&bpi,BPMSG_NUL,"OPT",szOpt);
				free (szOpt);
				binkp_state = BPI_WaitPWD;
			}
			break;
		
		case BPMSG_PWD:
			if( binkp_state == BPI_WaitPWD )
			{
				strnxcpy(remote_data->passwd, bpi.ibuf+1, sizeof(remote_data->passwd));
				binkp_state = BPI_Auth;
			}
			break;
		
		case BPMSG_ERR:
			log("BinkP error: \"%s\"", string_printable(bpi.ibuf+1));
			GOTO(Abort, HRC_FATAL_ERR);
			
		case BPMSG_BSY:
			log("BinkP busy: \"%s\"", string_printable(bpi.ibuf+1));
			GOTO(Abort, HRC_TEMP_ERR);
		}
	}

Exit:
	if( binkp_flush_queue(&bpi, bpi.timeout) && rc == HRC_OK )
		rc = HRC_OTHER_ERR;

Abort:
	binkp_deinit_bpinfo(&bpi);
	
	return rc;
}

int binkp_transfer(s_protinfo *pi) {

	int  i, n, rc = PRC_NOERROR;
	bool recv_ready = FALSE;
	bool send_ready = FALSE;
	bool rcvd_EOB = FALSE;
	bool recv_file = FALSE;
	int  recv_rc = 0;
	int  send_rc = 0;
	char *fname = NULL;
	size_t fsize = 0;
	time_t ftime = 0;
	size_t foffs = 0;
	s_bpinfo bpi;
  s_binkp_sysinfo *remote;
  enum {
    BPT_Start_Send_File,
    BPT_Wait_M_GET,
    BPT_Send_File,
    BPT_Wait_M_GOT,
    BPT_No_Files,
    BPT_EOB
  } binkp_send_state = BPT_Start_Send_File;
  remote = (s_binkp_sysinfo *) state.handshake->remote_data;
  binkp_init_bpinfo(&bpi);
  while (1) {
    if (binkp_send_state == BPT_Start_Send_File) {
      if (p_tx_fopen (pi)) {
        binkp_send_state = BPT_No_Files;
      } else {
	char *name  =        pi->send->net_name;
	long  total = (long) pi->send->bytes_total;
	long  time  = (long) pi->send->mod_time;
	if (remote->options & BINKP_OPT_NR) {
	  binkp_queuemsgf(&bpi,BPMSG_FILE,"%s %ld %ld -1",name,total,time);
	  binkp_send_state = BPT_Wait_M_GET;
	} else {
	  binkp_queuemsgf(&bpi,BPMSG_FILE,"%s %ld %ld 0", name,total,time);
	  binkp_send_state = BPT_Send_File;
	}
      }
    }
    if (binkp_send_state == BPT_Send_File) {
      if (bpi.opos == 0 && bpi.n_msgs == 0) {
	if((n = p_tx_readfile (bpi.obuf+BINKP_BLK_HDRSIZE,4096,pi))<0) {
	  p_tx_fclose (pi);
	  binkp_send_state = BPT_Start_Send_File;
	} else {
	  binkp_puthdr (bpi.obuf, (unsigned) (n & 0x7fff));
	  bpi.opos = n + BINKP_BLK_HDRSIZE;
	  pi->send->bytes_sent += n;
	  if (pi->send->eofseen) {
	    pi->send->status = FSTAT_WAITACK;
	    if (remote->options & BINKP_OPT_NR)
	         binkp_send_state = BPT_Wait_M_GOT;
	    else binkp_send_state = BPT_Start_Send_File;
	  }
	}
      }
    }
    if (binkp_send_state == BPT_No_Files) {
      for (i = 0; i < pi->n_sentfiles; i++) {
        if (pi->sentfiles[i].status == FSTAT_WAITACK) break;
      }
      if (i == pi->n_sentfiles) {
        binkp_queuemsg (&bpi, BPMSG_EOB, NULL, NULL);
        binkp_send_state = BPT_EOB;
      }
    }
    /* End of the current batch (start the next batch if need). */
    if (binkp_send_state == BPT_EOB && rcvd_EOB) {
      if (remote->options & BINKP_OPT_MB && bpi.msgs_in_batch > 2) {
	bpi.msgs_in_batch = 0;
	binkp_send_state = BPT_Start_Send_File;
	rcvd_EOB = FALSE;
	continue;
      }
      break;
    }
		recv_ready = send_ready = FALSE;
		if( tty_select(&recv_ready, (bpi.opos || bpi.n_msgs) ?
		               &send_ready : NULL, bpi.timeout) < 0 )
			gotoexit(PRC_ERROR);
		
		recv_rc = BPMSG_NONE;
		send_rc = 0;
		
		if( recv_ready && (recv_rc = binkp_recv(&bpi)) == BPMSG_EXIT )
			gotoexit(PRC_ERROR);
		if( send_ready && (send_rc = binkp_send(&bpi)) < 0 )
			gotoexit(PRC_ERROR);
	
    switch(recv_rc) {
      case BPMSG_NONE:
			break;
		case BPMSG_DATA: /* Got new data block */
			if( recv_file )
			{
				if( (n = p_rx_writefile(bpi.ibuf, bpi.isize, pi)) < 0 )
				{
					/* error writing file */
					if( n == -2 )
					{
						binkp_queuemsgf(&bpi, BPMSG_GOT, "%s %ld %ld",
							pi->recv->net_name, (long)pi->recv->bytes_total,
							(long)pi->recv->mod_time);
					}
					else
					{
						binkp_queuemsgf(&bpi, BPMSG_SKIP, "%s %ld %ld",
							pi->recv->net_name, (long)pi->recv->bytes_total,
							(long)pi->recv->mod_time);
					}
					recv_file = FALSE;
					p_rx_fclose(pi);
				}
				else
				{
					pi->recv->bytes_received += bpi.isize;

					/* Was it the last data block? */
					if( pi->recv->bytes_received > pi->recv->bytes_total )
					{
						log("binkp got too many data (%ld, %ld expected)",
							(long)pi->recv->bytes_received,
							(long)pi->recv->bytes_total);

						recv_file = FALSE;
						pi->recv->status = FSTAT_REFUSED;
						(void)p_rx_fclose(pi);
						
						binkp_queuemsgf(&bpi, BPMSG_SKIP, "%s %ld %ld",
							pi->recv->net_name, (long)pi->recv->bytes_total,
							(long)pi->recv->mod_time);
					}
					else if( pi->recv->bytes_received == pi->recv->bytes_total )
					{
						recv_file = FALSE;
						pi->recv->status = FSTAT_SUCCESS;
						if( !p_rx_fclose(pi) )
						{
							binkp_queuemsgf(&bpi, BPMSG_GOT, "%s %ld %ld",
								pi->recv->net_name, (long)pi->recv->bytes_total,
								(long)pi->recv->mod_time);
						}
						else
						{
							binkp_queuemsgf(&bpi, BPMSG_SKIP, "%s %ld %ld",
								pi->recv->net_name, (long)pi->recv->bytes_total,
								(long)pi->recv->mod_time);
						}
					}
				}
			}
#ifdef DEBUG
			else
				DEB((D_PROT, "ignore received data block"));
#endif
			break;
			
		case BPMSG_FILE:
			
			if( binkp_parsfinfo(bpi.ibuf+1, &fname,
			    &fsize, &ftime, &foffs) )
			{
				log ("BinkP error: M_FILE: %s", bpi.ibuf + 1);
				binkp_queuemsg(&bpi, BPMSG_ERR, "FILE: ", "unparsable arguments");
				goto FinishSession;
			}
			
			if( pi->recv && !p_compfinfo(pi->recv, fname, fsize, ftime)
			 && pi->recv->bytes_skipped == foffs && pi->recv->fp )
			{
				recv_file = TRUE;
				break;
			}
			
			if (recv_file) {
			  p_rx_fclose (pi);
			  recv_file = FALSE;
			}
			switch(p_rx_fopen(pi, fname, fsize, ftime, 0)) {
			case 0:
				if (pi->recv->bytes_skipped == foffs)
				{
					recv_file = TRUE;
					break;
				}
				binkp_queuemsgf(&bpi, BPMSG_GET, "%s %ld %ld %ld",
					pi->recv->net_name, (long)pi->recv->bytes_total,
					(long)pi->recv->mod_time,
					(long)pi->recv->bytes_skipped);
				break;
			case 1:
				/* SKIP (non-destructive) */
				binkp_queuemsgf(&bpi, BPMSG_SKIP, "%s %ld %ld",
					pi->recv->net_name, (long)pi->recv->bytes_total,
					(long)pi->recv->mod_time);
				break;
			case 2:
				/* SKIP (destructive) */
				binkp_queuemsgf(&bpi, BPMSG_GOT, "%s %ld %ld",
					pi->recv->net_name, (long)pi->recv->bytes_total,
					(long)pi->recv->mod_time);
				break;
			default:
				ASSERT_MSG();
			}
			break;
			
		case BPMSG_EOB: /* End Of Batch */
			if( recv_file )
			{
				p_rx_fclose(pi);
				recv_file = FALSE;
			}
			rcvd_EOB = TRUE;
			break;
			
      case BPMSG_GOT:
      case BPMSG_SKIP:
	if (binkp_parsfinfo (bpi.ibuf+1,&fname,&fsize,&ftime,NULL)) {
	  char *m = recv_rc == BPMSG_GOT ? "M_GOT" : "M_SKIP";
	  binkp_queuemsgf (&bpi, BPMSG_ERR, "%s: %s", m, bpi.ibuf + 1);
	  log ("BinkP error: %s: %s", m, bpi.ibuf + 1);
	  binkp_send_state = BPT_No_Files;
	  rc = PRC_ERROR;
	  break;
	}
	for(i = 0; i < pi->n_sentfiles; i++ ) {
	  if (!p_compfinfo (&pi->sentfiles[i], fname, fsize, ftime)) {
	    s_finfo *tmp = pi->send;
	    pi->send = &pi->sentfiles[i];
	    if (recv_rc == BPMSG_SKIP) {
	      pi->send->status = FSTAT_REFUSED;
	    } else {
	      if (pi->send->status == FSTAT_WAITACK) {
	        pi->send->status = FSTAT_SUCCESS;
	      } else {
		pi->send->status = FSTAT_SKIPPED;
	      }
	    }
	    p_tx_fclose(pi);
	    pi->send = tmp;
	    break;
	  }
	}
	if (!strcmp (pi->send->net_name, fname)) {
	  if (binkp_send_state == BPT_Send_File 	||
	      binkp_send_state == BPT_Wait_M_GET	||
	      binkp_send_state == BPT_Wait_M_GOT)	{
	      binkp_send_state =  BPT_Start_Send_File;
	  }
	}
	break;

		case BPMSG_ERR:
			log("remote report error: \"%s\"", bpi.ibuf+1);
			gotoexit(PRC_ERROR);
			break;
			
		case BPMSG_BSY:
			log("remote busy error: \"%s\"", bpi.ibuf+1);
			gotoexit(PRC_ERROR);
			break;
			
		case BPMSG_GET:
			if( binkp_parsfinfo(bpi.ibuf+1, &fname, &fsize, &ftime, &foffs) == 0 )
			{
				if(!p_compfinfo(pi->send,fname,fsize,ftime))
				{
					if( fseek(pi->send->fp, foffs, SEEK_SET) == -1 )
					{
						log("cannot send file from requested offset %ld", (long)foffs);
						p_tx_fclose(pi);
						binkp_send_state = BPT_Start_Send_File;
					}
					else
					{
						log("sending \"%s\" from %ld offset",
							pi->send->fname, (long)foffs);
						pi->send->bytes_skipped = foffs;
						pi->send->bytes_sent = foffs;
						binkp_queuemsgf(&bpi, BPMSG_FILE, "%s %ld %ld %ld",
							pi->send->net_name, (long)pi->send->bytes_total,
							(long)pi->send->mod_time, (long)foffs);
						binkp_send_state = BPT_Send_File;
					}
				}
			}
			break;
			
		default:
			log("binkp got unhandled msg #%d", recv_rc);
			break;
		}
	} /* end of while( !sent_EOB || !rcvd_EOB ) */
	
FinishSession:
	if( binkp_flush_queue(&bpi, bpi.timeout) && rc == PRC_NOERROR )
		rc = PRC_ERROR;
	
exit:
	if( pi->send && pi->send->fp ) p_tx_fclose(pi);
	if( pi->recv && pi->recv->fp ) p_rx_fclose(pi);

	binkp_deinit_bpinfo(&bpi);
	
	DEB((D_PROT, "binkp: BINKP exit = %d", rc));
	
	return rc;
}

