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
#include "bforce.h"
#include "util.h"
#include "nodelist.h"
#include "session.h"
#include "prot_zmodem.h"
#include "prot_yoohoo.h"
#include "prot_emsi.h"
#include "prot_binkp.h"

/* All session information stores here */
s_state state;

int hydra(s_protinfo *pi, bool flag_RH1);

s_faddr *session_get_bestaka(s_faddr addr)
{
	s_cval_entry *addr_ptr;
	s_cval_entry *hide_ptr;
	s_faddr *best = NULL;
	int bestl = 0;
	int curl = 0;

	for( addr_ptr = conf_first(cf_address); addr_ptr;
	     addr_ptr = conf_next(addr_ptr) )
	{
		for( hide_ptr = conf_first(cf_hide_our_aka); hide_ptr;
		     hide_ptr = conf_next(hide_ptr) )
		{
			if( !ftn_addrcomp(hide_ptr->d.falist.addr, addr_ptr->d.falist.addr) )
				break;
		}
		
		if( !hide_ptr )
		{
			curl = ftn_addrsmetric(addr_ptr->d.falist.addr, addr);
			
			if( curl > bestl || best == NULL )
			{
				bestl = curl;
				best  = &addr_ptr->d.falist.addr;
			}
		}
	}
	
	return best;
}

int session_addrs_lock(s_sysaddr *addrs, int anum)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	bool one_lock = FALSE;
	
	for( i = 0; i < anum; i++ )
	{
		if( addrs[i].good )
		{
#ifdef BFORCE_USE_CSY
			if( out_bsy_lock(addrs[i].addr, FALSE) )
#else
			if( out_bsy_lock(addrs[i].addr) )
#endif
			{
				log("exclude address %s: allready locked",
					ftn_addrstr(abuf, addrs[i].addr));
				addrs[i].busy = TRUE;
			}
			else
				one_lock = TRUE;
		}
	}

	return one_lock ? 0 : -1;
}

int session_addrs_add(s_sysaddr **addrs, int *anum, s_faddr addr)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	
	if( *anum && *addrs )
	{
		/* Check for addresses duplication */
		for( i = 0; i < *anum; i++ )
		{
			if( !ftn_addrcomp((*addrs)[i].addr, addr) )
			{
				log("exclude address %s: duplicated address",
					ftn_addrstr(abuf, addr));
				return -1;
			}
		}
	}
	
	if( *addrs && *anum )
		*addrs = xrealloc(*addrs, sizeof(s_sysaddr)*(*anum+1));
	else
		*addrs = xmalloc(sizeof(s_sysaddr));
	
	memset(&(*addrs)[*anum], '\0', sizeof(s_sysaddr));
	
	(*addrs)[*anum].addr = addr;
	(*addrs)[*anum].busy = FALSE;
	(*addrs)[*anum].good = FALSE;
	
	++(*anum);
	
	return 0;
}

int session_addrs_check(s_sysaddr *addrs, int anum, const char *passwd,
                        const char *challenge, int challenge_length)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	char pbuf[32];
	bool failure = FALSE;
	bool success = FALSE;
	bool cram = (challenge && *challenge);
	
	if( !anum )
		return -1;
	
	for( i = 0; i < anum; i++ )
	{
		if( session_check_addr(addrs[i].addr) )
		{
			log("exclude address %s: not acceptable",
				ftn_addrstr(abuf, addrs[i].addr));
			continue;
		}

		if( !session_get_password(addrs[i].addr, pbuf, sizeof(pbuf)) )
		{
			bool good_passwd = FALSE;

			if( passwd && *passwd )
			{
				if( cram )
				{
					char digest_bin[16];
					char digest_hex[33];
				
					md5_cram_get(pbuf, challenge, challenge_length, digest_bin);
			
					/* Encode digest to the hex string */
					string_bin_to_hex(digest_hex, digest_bin, 16);

					if( strcasecmp(passwd, digest_hex) == 0 )
						good_passwd = TRUE;
				}
				else if( strcasecmp(passwd, pbuf) == 0 )
					good_passwd = TRUE;
			}
			
			if( good_passwd )
			{
				/* correct password */
				addrs[i].good = TRUE;
				state.protected = TRUE;
				success = TRUE;
			}
			else
			{
				log("exclude address %s: bad password",
					ftn_addrstr(abuf, addrs[i].addr));
				addrs[i].good = FALSE;
				failure = TRUE;
			}
		}
		else
			/* not password protected address */
			addrs[i].good = TRUE;
	}

	/*
	 * Return error, if received password is incorrect for all AKAs
	 */
	if( failure && !success )
		return -1;

	return 0;
}

int session_addrs_to_falist(s_sysaddr *addrs, int anum, s_falist **dest)
{
	int i;
	
	for( i = 0; i < anum; i++ )
	{
		if( !addrs[i].busy && addrs[i].good )
		{
			(*dest) = (s_falist *)xmalloc(sizeof(s_falist));
			memset(*dest, '\0', sizeof(s_falist));
			(*dest)->addr = addrs[i].addr;
			dest = (s_falist**)&((*dest)->next);
		}
	}

	return 0;
}

int session_addrs_check_genuine(s_sysaddr *addrs, int anum, s_faddr expected)
{
	int i;

	for( i = 0; i < anum; i++ )
	{
		if( !ftn_addrcomp(addrs[i].addr, expected) )
			return 0;
	}

	return 1;
}

/* ------------------------------------------------------------------------- */
/* Return non-zero value if current speed too low                            */
/* ------------------------------------------------------------------------- */
int session_check_speed(void)
{
	state.minspeed = conf_number(cf_min_speed_in);
	
	if( state.connspeed > 0 && state.minspeed > state.connspeed )
		return 1;
	
	return 0;
}

int session_check_addr(s_faddr addr)
{
	s_cval_entry *addr_ptr;

	for( addr_ptr = conf_first(cf_address); addr_ptr;
	     addr_ptr = conf_next(addr_ptr) )
	{
		if( !ftn_addrcomp(addr, addr_ptr->d.falist.addr) )
			return 1;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/* Get session password for address $addr, put it in $buf                    */
/* If no password found - return non-zero value                              */
/* ------------------------------------------------------------------------- */
int session_get_password(s_faddr addr, char *buffer, size_t buflen)
{
	s_cval_entry *pwd_ptr;

	for( pwd_ptr = conf_first(cf_password); pwd_ptr;
	     pwd_ptr = conf_next(pwd_ptr) )
	{
		if( !ftn_addrcomp(addr, pwd_ptr->d.falist.addr) )
		{
			strnxcpy(buffer, pwd_ptr->d.falist.what, buflen);
			return 0;
		}
	}
	
	return 1;
}

int session_remote_lookup(s_sysaddr *addrs, int anum)
{
	int i;
	
	for( i = 0; i < anum; i++ )
	{
		if( addrs[i].good )
		{
			nodelist_lookup(&state.node, addrs[i].addr);
			state.node.addr.domain[0] = '\0';
			state.listed = state.node.listed;
			break;
		}
	}

	return (i < anum) ? 0 : -1;
}

void session_remote_log_status(void)
{
	log("remote is %s,%s",
		(state.listed) ? "listed" : "unlisted",
		(state.protected) ? "protected" : "unprotected");
}

/* ------------------------------------------------------------------------- */
/* Set inbound directory, create temporary inbound in it, check permissions  */
/* If something wrong - return non-zero value                                */
/* ------------------------------------------------------------------------- */
int session_set_inbound(void)
{
	struct stat st;
	char *p_inb;
	
	if( (p_inb = conf_string(cf_inbound_directory)) )
	{
		state.inbound = (char*)xstrcpy(p_inb);
	}
	else
	{
		log("no inbound specified, assume \"./\"");
		state.inbound = (char*)xstrcpy("./");
	}

	state.tinbound = (char*)xstrcpy(state.inbound);
	state.tinbound = (char*)xstrcat(state.tinbound, "tmp/");
	
	/*
	 * Warning, access() make checks using real uid and gid
	 * (not effective), so we can fail, but..
	 */
	if( stat(state.tinbound, &st) == 0 )
	{
		if( (st.st_mode & S_IFDIR) != S_IFDIR )
		{
			log("temporary inbound \"%s\" is not directory", state.tinbound);
			return(1);
		}
		else if( access(state.tinbound, R_OK|W_OK|X_OK) )
		{
			log("have no r/w permission to temporary inbound \"%s\"", state.tinbound);
			return(1);
		}
	}
	else if( errno == ENOENT )
	{
		/* "tmp" inbound doesn't exist */
		if( mkdir(state.tinbound, 0700) < 0 )
		{
			logerr("can't create temporary inbound \"%s\"", state.tinbound);
			return(1);
		}
		chmod(state.tinbound, 0700);
	}
	else
	{
		/* Different stat() errors */
		logerr("can't stat temporary inbound \"%s\"", state.tinbound);
		return 1;
	}
	
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Set status of _OUR_ FREQ processor                                        */
/* ------------------------------------------------------------------------- */
void session_set_freqs_status(void)
{
	int root_ok = 0;
	int magc_ok = 0;
	char *p;
	long options;
	
	state.reqstat = REQS_DISABLED;

	options = conf_options(cf_options);
	
	if( (options & OPTIONS_MAILONLY) == OPTIONS_MAILONLY
	 || (options & OPTIONS_NO_FREQS) == OPTIONS_NO_FREQS )
		{ state.reqstat = REQS_NOTALLOW; return; }
	
	if( state.connspeed > 0
	 && state.connspeed < conf_number(cf_freq_min_speed) )
		{ state.reqstat = REQS_NOTALLOW; return; }
	
	if( (p = conf_string(cf_freq_srif_command)) && *p )
	{
		/*
		 * Can we execute this processor? Disable freqs if we can't!
		 */
		if( !exec_file_exist(p) )
			state.reqstat = REQS_ALLOW;
		else
			logerr("can't stat SRIF processor \"%s\"", p);
	}
	else
	{
		/* Check root dir */
		if( (p = conf_string(cf_freq_dir_list)) && *p )
		{
			if( access(p, R_OK) == 0 )
				root_ok = 1;
			else
				logerr("can't stat FREQ dir list \"%s\"", p);
		}
		
		/* Check magic dir */
		if( (p = conf_string(cf_freq_alias_list)) && *p )
		{
			if( access(p, R_OK) == 0 )
				magc_ok = 1;
			else
				logerr("can't stat FREQ alias list \"%s\"", p);
		}
		
		/* Set FREQ processor status */
		if( root_ok || magc_ok )
			state.reqstat = REQS_ALLOW;
	}
}

/* ------------------------------------------------------------------------- */
/* Set our "send options" (common part, based on our local settings)         */
/* ------------------------------------------------------------------------- */
void session_set_send_options(void)
{
	const long options = conf_options(cf_options);
	
	if( state.caller == FALSE )
		state.sopts.holdreq = 1;
		
	if( (options & OPTIONS_MAILONLY) == OPTIONS_MAILONLY )
		state.sopts.holdxt = 1;
	if( (options & OPTIONS_HOLDXT) == OPTIONS_HOLDXT )
		state.sopts.holdxt = 1;
	if( (options & OPTIONS_HOLDREQ) == OPTIONS_HOLDREQ )
		state.sopts.holdreq = 1;
	if( (options & OPTIONS_HOLDALL) == OPTIONS_HOLDALL )
		state.sopts.holdall = 1;
	if( (options & OPTIONS_HOLDHOLD) == OPTIONS_HOLDHOLD )
		state.sopts.holdhold = 1;
}

/* ------------------------------------------------------------------------- */
/* Return (non-zero) if we _CAN'T_ send this file NOW!                       */
/* ------------------------------------------------------------------------- */
static int holdfile(s_filelist *fi, const char *delayout)
{
	if( state.sopts.holdall                               ) return(1);
	if( state.sopts.holdxt  && !(fi->type & TYPE_NETMAIL) ) return(1);
	if( state.sopts.holdreq &&  (fi->type & TYPE_REQUEST) ) return(1);
	if( state.sopts.holdhold && fi->flavor == FLAVOR_HOLD ) return(1);
	if( fi->status != STATUS_WILLSEND                     ) return(1);
	if( delayout && !checkmasks(delayout, fi->fname)      ) return(2);
	
	return 0;
}

/* ------------------------------------------------------------------------- */
/* Set SKIP flag for files we can't send NOW                                 */
/* ------------------------------------------------------------------------- */
void session_set_send_files(void)
{
	int holdmsg = 0, rc = 0;
	s_filelist *ptrl = NULL;

	const char *delayout = conf_string(cf_delay_files_send);
	
	for( ptrl = state.queue.fslist; ptrl; ptrl = ptrl->next )
		if( (rc = holdfile(ptrl, delayout)) )
		{
			if( rc == 2 && !holdmsg )
			{
				log("delaying files \"%s\"", delayout);
				++holdmsg;
			}
			ptrl->status = STATUS_SKIP;
		}
}

int session_create_files_queue(s_sysaddr *addrs, int anum)
{
	s_falist *mailfor = NULL;
	s_outbound_callback_data ocb;
			
	/* Set addresses for which we will send files */
	(void)session_addrs_to_falist(addrs, anum, &mailfor);
	
	/* Scan outbound directory */
	if( mailfor )
	{
		memset(&ocb, '\0', sizeof(s_outbound_callback_data));
		ocb.callback = out_handle_fsqueue;
		ocb.dest = (void *)&state.queue;
		(void)out_scan(&ocb, mailfor);
		(void)session_set_send_files();
		(void)session_traffic_set_outgoing(&state.traff_send);
	}

	return 0;
}

int session_traffic_set_incoming(s_traffic *dest)
{
	memset(dest, '\0', sizeof(s_traffic));
	
	if( state.handshake && state.handshake->remote_traffic )
		return state.handshake->remote_traffic(state.handshake, dest);

	return -1;
}

int session_traffic_set_outgoing(s_traffic *dest)
{
	s_filelist *ptrl;

	memset(dest, '\0', sizeof(s_traffic));
	
	for( ptrl = state.queue.fslist; ptrl; ptrl = ptrl->next )
	{
		if( ptrl->type & TYPE_NETMAIL )
		{
			dest->netmail_size += ptrl->size;
			dest->netmail_num++;
		}
		else if( ptrl->type & TYPE_ARCMAIL )
		{
			dest->arcmail_size += ptrl->size;
			dest->arcmail_num++;
		}
		else
		{
			dest->files_size += ptrl->size;
			dest->files_num++;
		}
	}

	return 0;
}

void session_traffic_log(bool incoming, s_traffic *traff)
{
	char buf[32];
	char msg[128] = "";
	
	if( traff == NULL )
		strcpy(msg, "unknown");
	else if( traff->netmail_size == 0
	      && traff->arcmail_size == 0 && traff->files_size == 0 )
		strcpy(msg, "none");
	else
	{
		if( traff->netmail_size > 0 )
		{
			string_humansize(buf, traff->netmail_size);
			strcat(msg, buf);
			strcat(msg, " netmail, ");
		}
		if( traff->arcmail_size > 0 )
		{
			string_humansize(buf, traff->arcmail_size);
			strcat(msg, buf);
			strcat(msg, " arcmail, ");
		}
		if( traff->files_size > 0 )
		{
			string_humansize(buf, traff->files_size);
			strcat(msg, buf);
			strcat(msg, " files, ");
		}
		if( *msg )
			msg[strlen(msg)-2] = '\0';
	}

	log("%s traffic: %s", incoming ? "incoming" : "outgoing", msg);
}

void session_traffic(void)
{
	int rc;
	
	rc = session_traffic_set_incoming(&state.traff_recv);
	session_traffic_log(TRUE,  rc ? NULL : &state.traff_recv);

	/* Outgoing traffic must be allread calculated */
	session_traffic_log(FALSE, &state.traff_send);
}

/*
 * History file line format:
 *   line verbal name,
 *   remote address,
 *   session start time (Unix),
 *   session_length (seconds),
 *   session status flags (L - listed, P - protected),
 *   session result code (one of mailer return codes),
 *   size of sent netmail,
 *   size of sent arcmail,
 *   size of sent files,
 *   size of received netmail,
 *   size of received arcmail,
 *   size of received files
 */
void session_update_history(s_traffic *send, s_traffic *recv, int rc)
{
	FILE *hist_fp;
	char *hist_file = conf_string(cf_history_file);
	char  abuf[BFORCE_MAX_ADDRSTR+1];
	char  session_status[32] = "";

	if( !hist_file )
		return;

	hist_fp = file_open(hist_file, "a");
	if( !hist_fp )
	{
		logerr("cannot open history file \"%s\"", hist_file);
		return;
	}


	if( state.listed )
		strcat(session_status, "L");
	if( state.protected )
		strcat(session_status, "P");
	if( state.caller )
		strcat(session_status, "O");
	else
		strcat(session_status, "I");
	
	fprintf(hist_fp, "%s,%s,%lu,%u,%s,%d,%lu,%lu,%lu,%lu,%lu,%lu\n",
		state.linename ? state.linename : "",
		state.node.addr.zone ? ftn_addrstr(abuf, state.node.addr) : "",
		(unsigned long) state.start_time,
		(unsigned int)  time_elapsed(state.start_time),
		session_status,
		rc,
		(unsigned long) send->netmail_size,
		(unsigned long) send->arcmail_size,
		(unsigned long) (send->files_size + send->freqed_size),
		(unsigned long) recv->netmail_size,
		(unsigned long) recv->arcmail_size,
		(unsigned long) (recv->files_size + recv->freqed_size));
		
	
	(void)file_close(hist_fp);
}

/* ------------------------------------------------------------------------- */
/* Start session with another FTN mailer                                     */
/* ------------------------------------------------------------------------- */
int session(void)
{
	s_protinfo pi;
	int rc = BFERR_NOERROR;
	s_traffic traff_send;
	s_traffic traff_recv;
	char *p;

	memset(&traff_send, '\0', sizeof(s_traffic));
	memset(&traff_recv, '\0', sizeof(s_traffic));

	/* Store session start time */
	state.start_time = time(NULL);

	if( state.session == SESSION_UNKNOWN )
	{
		rc = state.caller ? session_init_outgoing()
		                  : session_init_incoming();
		
		if( rc )
			gotoexit(BFERR_HANDSHAKE_ERROR);
	}

	/* -------------------------------------------------------------- */	
	/* Handshake part                                                 */
	/* -------------------------------------------------------------- */	
	switch( state.session ) {
	case SESSION_EMSI:
		state.handshake = &handshake_protocol_emsi;
		break;
	case SESSION_BINKP:
		state.handshake = &handshake_protocol_binkp;
		break;
	case SESSION_YOOHOO:
		state.handshake = &handshake_protocol_yoohoo;
		break;
	case SESSION_FTSC:
		log("%sbound FTS-1 session", state.caller?"out":"in");
		log("FTS-1 session not availabe");
		gotoexit(BFERR_HANDSHAKE_ERROR);
	case SESSION_UNKNOWN:
		ASSERT_MSG();
		gotoexit(BFERR_HANDSHAKE_ERROR);
	default:
		ASSERT_MSG();
		gotoexit(BFERR_HANDSHAKE_ERROR);
	}
	
	state.handshake->init(state.handshake);
	
	log("%sbound %s session",
		state.caller ? "out" : "in", state.handshake->verbal_name);
	
	rc = state.caller ? state.handshake->outgoing_session(state.handshake)
	                  : state.handshake->incoming_session(state.handshake);

	DEB((D_PROT, "session: handshake rc = %d", rc));
	
	if( rc != HRC_OK )
	{
		const char *errmsg = NULL;
			
		switch(rc) {
		case HRC_LOW_SPEED:
			errmsg = "connect speed too low";
			rc = BFERR_CONNECT_TOOLOW;
			break;
		case HRC_BAD_PASSWD:
			errmsg = "security violation";
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		case HRC_NO_ADDRESS:
			errmsg = "no expected address was presented";
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		case HRC_NO_PROTOS:
			errmsg = "no common protocols";
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		case HRC_BUSY:
			errmsg = "all remote addresses are busy";
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		case HRC_FATAL_ERR:
		case HRC_TEMP_ERR:
		case HRC_OTHER_ERR:
			errmsg = NULL;
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		default:
			errmsg = "unexpected error number";
			rc = BFERR_HANDSHAKE_ERROR;
			break;
		}

		if( errmsg )
			log("abort session due to: %s", errmsg);
	}
	else
	{
		/*
		 * Execute 'run_after_handshake' command
		 */
		if( (p = conf_string(cf_run_after_handshake)) )
			session_run_command(p);
		
		/*
		 * Files transfer part
		 */
		DEB((D_FREQ, "setreqstat: Our FREQ processor status: \"%s\"",
				( state.reqstat == REQS_ALLOW    ) ? "Allowed":
				( state.reqstat == REQS_NOTALLOW ) ? "Not allowed now":
				( state.reqstat == REQS_DISABLED ) ? "No FREQs available":"Error"));
		DEB((D_HSHAKE, "session: decided to use %s protocol",
				Protocols[state.handshake->protocol]));
		
		/*
		 * Log expected traffic
		 */
		session_traffic();
		
		init_protinfo(&pi, state.caller);
	
		switch(state.handshake->protocol) {
		case PROT_BINKP:
			rc = binkp_transfer(&pi);
			break;
		case PROT_ZMODEM:
		case PROT_ZEDZAP:
		case PROT_DIRZAP:
			rc = state.caller ? tx_zmodem(&pi, state.caller)
			                  : rx_zmodem(&pi, state.caller);
			if( rc == PRC_NOERROR )
			{
				rc = state.caller ? rx_zmodem(&pi, state.caller)
				                  : tx_zmodem(&pi, state.caller);
			}
			break;
		case PROT_JANUS:
			log("Janus is not available in current version");
			break;
		case PROT_HYDRA:
			rc = hydra(&pi, state.sopts.hydraRH1);
			break;
		case PROT_NOPROT:
			log("no common protocols available");
			break;
		default:
			ASSERT_MSG();
			break;
		}
		
		/*
		 * Convert value returned by protocol to the BForce return code
		 */
		switch(rc) {
		case PRC_NOERROR:       rc = BFERR_NOERROR; break;
		case PRC_ERROR:
		case PRC_REMOTEABORTED:
		case PRC_LOCALABORTED:  rc = BFERR_XMITERROR; break;
		case PRC_CPSTOOLOW:     rc = BFERR_CPSTOOLOW; break;
		case PRC_STOPTIME:      rc = BFERR_STOPTIME; break;
		default:                ASSERT_MSG();
		}
		state.session_rc = rc;
		
		/*
		 * Do session clenup (remove temp. files, etc.)
		 */
		(void)p_session_cleanup(&pi, (rc == BFERR_NOERROR));
		
		if( rc == BFERR_NOERROR )
		{
			/*
			 * Flush our 'stdout' buffer
			 */
			FLUSHOUT();
			
			/*
			 * Remove empty .?lo files
			 */
			(void)out_flo_unlinkempty(state.queue.flotab, state.queue.flonum);

			/*
			 * Wait a little if we are answering on incoming call
			 * (to be sure that all data will be sent) (?)
			 */
			if( !state.caller ) sleep(1);
		}
		
		/*
		 * Remove all .bsy locks
		 */
		out_bsy_unlockall();
		
		/*
		 * Write total amount of received/sent bytes, files, etc.
		 */
		p_log_txrxstat(&pi);

		/*
		 * Save session traffic before deiniting
		 */
		traff_send = pi.traffic_sent;
		traff_recv = pi.traffic_rcvd;
		
		deinit_protinfo(&pi);

		/*
		 * Execute 'run_after_session' command
		 */
		if( (p = conf_string(cf_run_after_session)) )
			session_run_command(p);
	}
	
exit:
	state.session_rc = rc;
	session_update_history(&traff_send, &traff_recv, rc);
	
	return rc;
}
