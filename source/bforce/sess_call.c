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
#include "nodelist.h"
#include "io.h"
#include "session.h"

#ifdef XXXX
/* Unused */
int call_system_modem_init(TIO *old_tio_settings)
{
	int   rc = BFERR_NOERROR;
	char  abuf[BF_MAXADDRSTR+1];
	int   c;
	TIO   oldtio;                  /* Original terminal settings */
	long  minspeed = 0L;           /* Min connect speed */
	long  waittime = 0L;           /* Abort dialing after.. */	
	char  dialstring[256]  = "";   /* Modem dial string with phone number */
	char  phone[BFORCE_MAX_PHONE+1];
	const char *p_resetstr = NULL; /* Modem reset string */
	const char *p_prefstr  = NULL; /* Modem dial prefix string */
	const char *p_suffstr  = NULL; /* Modem dial suffix string */

	/*
	 * Set verbal line name to the modem device name
	 */
	state.linename = port_get_name(state.modemport->name);
	
	/*
	 * Open new log file with current line name as extension
	 */
	if( log_reopen(log_getfilename(LOG_FILE_SESSION),
	               state.linename, NULL) )
	{
		log("can't continue without logging");
		return BFERR_FATALERROR;
	}

#ifdef DEBUG
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	if( (rc = port_open(state.modemport, 1, old_tio_setting)) )
	{
		log("cannot open modem port \"%s\"",
				state.modemport->name);
		return BFERR_PORTBUSY;
	}
	
	/* Load dialing options */
	p_resetstr = conf_string(cf_modem_reset_command);
	p_prefstr  = conf_string(cf_modem_dial_prefix);
	p_suffstr  = conf_string(cf_modem_dial_suffix);
	waittime   = conf_number(cf_wait_carrier_out);

	/* Set default wait for carrier time */
	if( !waittime )
		waittime = 120;
	
	/* Prepare dial string */
	*dialstring = '\0';
	if( p_prefstr )
		strnxcpy(dialstring, p_prefstr, sizeof(dialstring));
	else
		log("warning: no modem dial prefix defined");

	modem_transphone(phone, state.node.phone, sizeof(phone));
	strnxcat(dialstring, phone, sizeof(phone));
	
	if( p_suffstr )
		strnxcat(dialstring, p_suffstr, sizeof(dialstring));
	else
		log("warning: no modem dial suffix defined");

	log("calling %s (%s, %s)",
		ftn_addrstr(abuf, state.node.addr),
		(state.node.name && *state.node.name) ? state.node.name  : "<none>",
		string_printable(dialstring));
		
	/* Clear modem buffer from possible "NO CARRIER"s */
	CLEARIN();

	/* Reset modem */
	if( p_resetstr && *p_resetstr )
	{
		if( (c = modem_command(p_resetstr, 5, TRUE)) )
		{
			log("can't reset modem: %s", modem_errlist[c]);
			port_deinit(0, &oldtio);
			port_close();
			return BFERR_NOMODEM;  /* XXX close port? */
		}
		/* Try to remove some needless
		 * modem responses from buffer */
		CLEARIN();
	}
	
	rc = modem_dial(dialstring, waittime, &state.connstr);
	
	if( rc )
	{
		/* Error must be allready reported */
		port_deinit(0, &oldtio);
		port_close();
		return BFERR_CANT_CONNECT10;
	}
	
	if( state.connstr )
	{
		state.connspeed = modem_getconnspeed(state.connstr);
		log("connect \"%s\" (%ld)",
				state.connstr, state.connspeed);
	}
	
	minspeed = conf_number(cf_min_speed_out);
	if( state.connspeed && state.connspeed < minspeed )
	{
		TTYRESET();
		log("connect speed too low, at least %ld required",
				minspeed);
		port_deinit(0, &oldtio);
		port_close();
		return BFERR_CONNECT_TOOLOW;
	}
	
	/* Give modem some time for raising DCD line */
	usleep(100000); /* 0.1 sec */
	
	if( tio_get_dcd(0) == 0 )
		log("warning: DCD line is not active after connect!");
				
	port_carrier(0, TRUE);

	return BFERR_NOERROR;
}

/* Unused */
int call_system_modem_deinit(TIO *old_tio_settings)
{
	const char *p_hangstr; /* Modem hangup string */
	const char *p_statstr; /* Modem statistic string */
	
	p_hangstr  = conf_string(cf_modem_hangup_command);
	p_statstr  = conf_string(cf_modem_stat_command);

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	CLEARIN(); CLEAROUT();
	
	/* Hangup if we think modem is still online! */
	if( !tty_hangup && p_hangstr && *p_hangstr )
	{
		TTYRESET(); /* Reset error flags */
	
		if( (c = modem_hangup(p_hangstr, 5)) )
			log("can't hangup modem: %s", modem_errlist[c]);
	}

	/* Write modem statistic to log file */
	if( p_statstr && *p_statstr )
	{
		TTYRESET(); /* Reset error flags */
	
		if( (c = modem_command(p_statstr, 10, TRUE)) )
			log("can't get modem statistic: %s", modem_errlist[c]);
	}
	
	port_deinit(0, old_tio_settings);
	port_close();

	return BFERR_NOERROR;
}
#endif /* XXXX */

int call_system_quiet(const char *connstr, bool inet)
{
	TIO oldtio;
	struct sockaddr_in client;
	int clientlen = sizeof(client);
	int rc = 0;
	char *p;

	
	char *exec_cmd;
	int exec_result;

	if( (exec_cmd = conf_string(cf_run_before_session)) != NULL )
	{
	     exec_result = system(exec_cmd);
	     if( exec_result = 0  )
		  log("external application %s executed with zero return code (%i)", exec_cmd, exec_result);
	     else
		  logerr("external application %s executed with non-zero return code %i", exec_cmd, exec_result);
	}
	
	/*
	 * Set verbal line name
	 */
	if( inet ) {
		state.linename = xstrcpy("tcpip");
		state.inet = TRUE;
	}
	else
		state.linename = isatty(0) ? port_get_name(ttyname(0)) : NULL;
	
	if( !inet )
	{
		if( tio_get_dcd(0) == 0 )
			log("warning: DCD line is not active");
		
		if( connstr && *connstr )
			state.connstr = (char*)xstrcpy(connstr);
		else if( (p = getenv("CONNECT")) && *p )
			state.connstr = (char*)xstrcpy(p);
		
		if( state.connstr )
			state.connspeed = modem_getconnspeed(state.connstr);
	}
	
	/*
	 * Open new log file with current line name as extension
	 */
	if( log_reopen(log_getfilename(LOG_FILE_SESSION), state.linename, NULL) )
	{
		log("can't continue without logging");
		return BFERR_FATALERROR;
	}

#ifdef DEBUG
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	if( inet )
	{
		if( connstr && *connstr )
			state.connstr = (char*)xstrcpy(connstr);
		else if( getpeername(0, (struct sockaddr*)&client, &clientlen) == -1 )
			logerr("can't get client address");
		else
		{
			state.peername = (char*)xstrcpy(inet_ntoa(client.sin_addr));
			state.peerport = (long)ntohs(client.sin_port);
		}
	}
	
	if( !inet && state.cidstr )
	{
		setproctitle("bforce outgoing, CID: %.32s",
			string_printable(state.cidstr));
		log("Caller-ID: \"%s\"",
			string_printable(state.cidstr));
	}
	else if( inet && state.peername )
	{
		setproctitle("bforce outgoing, host %.32s:%ld",
			string_printable(state.peername), state.peerport);
		log("TCP/IP connect from %s on port %ld",
			string_printable(state.peername), state.peerport);
	}
	else
	{
		setproctitle("bforce outgoing");
	}
	
	if( state.connstr )
		log("connect \"%s\" (%ld)", state.connstr, state.connspeed);

	if( (!inet && (rc = port_init(0, 0, &oldtio, FALSE)) == 0)
	 || ( inet && (rc = tcpip_init()) == 0) )
	{
		port_carrier(0, TRUE);
		
		rc = session();
		
		if( !inet )
		{
			port_deinit(0, &oldtio);
			port_close();
		}
	}
	else
		rc = BFERR_FATALERROR;

	return rc;
}

/* ------------------------------------------------------------------------- */
/*  Before using we must lock tty and set:                                   */
/*    state.node      - node information                                     */
/*    state.modemport - to modem device we are going to use                  */
/* ------------------------------------------------------------------------- */
int call_system_modem(void)
{
	char  abuf[BF_MAXADDRSTR+1];
	int   c, rc = BFERR_NOERROR;
	TIO   oldtio;                  /* Original terminal settings */
	long  minspeed = 0L;           /* Min connect speed */
	long  waittime = 0L;           /* Abort dialing after.. */	
	char  dialstring[256]  = "";   /* Modem dial string with phone number */
	char  phone[BFORCE_MAX_PHONE+1];
	const char *p_resetstr = NULL; /* Modem reset string */
	const char *p_prefstr  = NULL; /* Modem dial prefix string */
	const char *p_suffstr  = NULL; /* Modem dial suffix string */
	const char *p_hangstr  = NULL; /* Modem hangup string */
	const char *p_statstr  = NULL; /* Modem statistic string */

	/*
	 * Set verbal line name to the modem device name
	 */
	state.linename = port_get_name(state.modemport->name);
	
	/*
	 * Open new log file with current line name as extension
	 */
	if( log_reopen(log_getfilename(LOG_FILE_SESSION),
	               state.linename, NULL) )
	{
		log("can't continue without logging");
		return BFERR_FATALERROR;
	}

#ifdef DEBUG
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	if( (rc = port_open(state.modemport, 1, &oldtio)) == 0 )
	{
		/* Load dialing options */
		p_resetstr = conf_string(cf_modem_reset_command);
		p_prefstr  = conf_string(cf_modem_dial_prefix);
		p_suffstr  = conf_string(cf_modem_dial_suffix);
		p_hangstr  = conf_string(cf_modem_hangup_command);
		p_statstr  = conf_string(cf_modem_stat_command);
		waittime   = conf_number(cf_wait_carrier_out);
	
		/* Set default wait for carrier time */
		if( !waittime )
			waittime = 120;
		
		*dialstring = '\0';
		
		if( p_prefstr )
			strnxcpy(dialstring, p_prefstr, sizeof(dialstring));
		else
			log("warning: no modem dial prefix defined");
		
		modem_transphone(phone, state.node.phone, sizeof(phone));
		strnxcat(dialstring, phone, sizeof(phone));
		
		if( p_suffstr )
			strnxcat(dialstring, p_suffstr, sizeof(dialstring));
		else
			log("warning: no modem dial suffix defined");
		
		log("calling %s (%s, %s)",
			ftn_addrstr(abuf, state.node.addr),
			(state.node.name && *state.node.name) ? state.node.name  : "<none>",
			string_printable(dialstring));
		
		/*
		 * Clear modem buffer from possible "NO CARRIER"s
		 */
		CLEARIN();
	
		/*
		 * Reset modem
		 */
		if( p_resetstr && *p_resetstr )
		{
			if( (c = modem_command(p_resetstr, 5, TRUE)) )
			{
				log("can't reset modem: %s", modem_errlist[c]);
				return BFERR_NOMODEM; /* XXX close port */
			}
			/*
			 * Try to remove some needless
			 * modem responses from buffer
			 */
			CLEARIN();
		}
		
		rc = modem_dial(dialstring, waittime, &state.connstr);
		
		if( rc < 0 )
		{
			rc = BFERR_CANT_CONNECT10;
		}
		else if( rc == 0 )
		{
			if( state.connstr )
			{
				state.connspeed = modem_getconnspeed(state.connstr);
				log("connect \"%s\" (%ld)", state.connstr, state.connspeed);
			}
			
			minspeed = conf_number(cf_min_speed_out);
			
			if( state.connspeed && state.connspeed < minspeed )
			{
				TTYRESET();
				log("connect speed too low, at least %ld required", minspeed);
				rc = BFERR_CONNECT_TOOLOW;
			}
			else
			{
				/* Give modem some time for rising DCD line */
				usleep(100000); /* 0.1 sec */
				
				if( tio_get_dcd(0) == 0 )
					log("warning: DCD line is not active after connect!");
				
				port_carrier(0, TRUE);
				
				rc = session();
				
				port_carrier(0, FALSE);
			}

			signal(SIGHUP, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			signal(SIGTERM, SIG_IGN);

			CLEARIN(); CLEAROUT();
			
			/* Hangup if we think modem is still online! */
			if( !tty_hangup && p_hangstr && *p_hangstr )
			{
				TTYRESET(); /* Reset error flags */
				
				if( (c = modem_hangup(p_hangstr, 5)) )
					log("can't hangup modem: %s", modem_errlist[c]);
			}
			
			/* Write modem statistic to log file */
			if( p_statstr && *p_statstr )
			{
				TTYRESET(); /* Reset error flags */
				
				if( (c = modem_command(p_statstr, 10, TRUE)) )
					log("can't get modem statistic: %s", modem_errlist[c]);
			}
		}
		port_deinit(0, &oldtio);
		port_close();
	}
	else
	{
		log("cannot open modem port \"%s\"", state.modemport->name);
		rc = BFERR_PORTBUSY;
	}

	return rc;
}

int call_system_tcpip(void)
{
	char abuf[BF_MAXADDRSTR+1];
	int rc = BFERR_NOERROR;
	
	/*
	 * Set verbal line name to "tcpip" value
	 */
	state.linename = xstrcpy("tcpip");
	
	state.inet = TRUE;
	
	/*
	 * Open new log file with current line name as extension
	 */
	if( log_reopen(log_getfilename(LOG_FILE_SESSION),
	               state.linename, NULL) )
	{
		log("can't continue without logging");
		return BFERR_FATALERROR;
	}

#ifdef DEBUG
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	if( nodelist_checkflag(state.node.flags, "BINKP") == 0 
	 || nodelist_checkflag(state.node.flags, "IBN") == 0 )
	{
		state.tcpmode = TCPMODE_BINKP;
		state.session = SESSION_BINKP;
	}
	else if( nodelist_checkflag(state.node.flags, "TELN") == 0 )
	{
		state.tcpmode = TCPMODE_TELNET;
		state.session = SESSION_UNKNOWN;
	}
	else if( nodelist_checkflag(state.node.flags, "IFC") == 0 )
	{
		state.tcpmode = TCPMODE_RAW;
		state.session = SESSION_UNKNOWN;
	}
	else /* Default is "raw" mode */
	{
		state.tcpmode = TCPMODE_RAW;
		state.session = SESSION_UNKNOWN;
	}

	log("calling %s (%s, %s)",
		ftn_addrstr(abuf, state.node.addr),
		(state.node.name  && *state.node.name ) ? state.node.name  : "<none>",
		(state.node.phone && *state.node.phone) ? state.node.phone : "<none>");
		
	if( (rc = tcpip_connect(state.node.phone, state.tcpmode)) == 0
	 && (rc = tcpip_init() == 0) )
	{
		TTYSTATUS(1);
		rc = session();
		tcpip_shutdown();
	}
	else
	{
		rc = BFERR_CANT_CONNECT10;
	}

	return rc;
}

int call_system(s_faddr addr, const s_bforce_opts *opts)
{
        int rc = 0;
        int runrc = 0;
	char abuf[BF_MAXADDRSTR+1];
	char *p_lockdir = NULL;
	char *errmsg = NULL;
	bool inet = FALSE;

	init_state(&state);

	state.caller    = TRUE;
	state.valid     = TRUE;
	state.node.addr = addr;
	nodelist_lookup(&state.node, addr);
	state.listed    = state.node.listed;
	state.node.addr.domain[0] = '\0'; /* Discard domain for node address */

	if( opts->dontcall )
		goto do_session;

	/*
	 * Get node overrides
	 */
	if( override_get(&state.override, state.node.addr, opts->hiddline) )
	{
		errmsg = "incorrect hidden line number";
		gotoexit(BFERR_PHONE_UNKNOWN);
	}

	/*
	 * Apply overrides to the node information
	 */	
	if( state.override.sFlags )
	{
		strnxcat(state.node.flags, ",", sizeof(state.node.flags));
		strnxcat(state.node.flags, state.override.sFlags, sizeof(state.node.flags));
	}
	
	if( opts->iaddr && *opts->iaddr )
	{
		inet = TRUE;
		(void)strnxcpy(state.node.phone,
				opts->iaddr, sizeof(state.node.phone));
	}
	else if( opts->phone && *opts->phone )
	{
		(void)strnxcpy(state.node.phone,
				opts->phone, sizeof(state.node.phone));
	}
	else if( state.override.sIpaddr )
	{
		inet = TRUE;
		(void)strnxcpy(state.node.phone,
				state.override.sIpaddr, sizeof(state.node.phone));
	}
	else if( state.override.sPhone )
	{
		(void)strnxcpy(state.node.phone,
				state.override.sPhone, sizeof(state.node.phone));
	}
	
	if( !inet )
	{
		if( !modem_isgood_phone(state.node.phone) )
			errmsg = "don't know phone number";
	}
	else
	{
		if( !tcpip_isgood_host(state.node.phone) )
			errmsg = "don't know host name";
	}
	
	if( errmsg )
		gotoexit(BFERR_PHONE_UNKNOWN);
	
	/*
	 * Is now a working time for that node/line
	 */
	if( !inet && !opts->force)
	{
		time_t unixtime = time(NULL);
		struct tm *now = localtime(&unixtime);
		bool goodtime;
		
		if( !opts->hiddline )
		{
			if( state.override.sFlags && !nodelist_checkflag(state.override.sFlags, "CM") )
				goodtime = TRUE;
			else
			{
				if( timevec_isdefined(&state.override.worktime) )
					goodtime = timevec_isnow(&state.override.worktime, now);
				else
				{
					if( !nodelist_checkflag(state.node.flags, "CM") )
						goodtime = TRUE;
					else
						goodtime = timevec_isnow(&state.node.worktime, now);
				}
			}
		}
		else
			goodtime = timevec_isnow(&state.override.worktime, now);
		
		if( !goodtime )
		{
			errmsg = "not works now, try later";
			gotoexit(BFERR_NOTWORKING);
		}
	}
	
do_session:
	/*
	 * It's easier to ignore than handle! After connect
	 * new handlers must be installed, don't worry.
	 */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	
	/*
	 * Try to lock address of system we are going to call
	 */
#ifdef BFORCE_USE_CSY
	if( out_bsy_lock(state.node.addr, TRUE) )
#else
	if( out_bsy_lock(state.node.addr) )
#endif
	{
		errmsg = "cannot lock address";
		gotoexit(BFERR_SYSTEM_LOCKED);
	}
	if( NULL != state.override.run && strlen(state.override.run) > 0 )
	{
	     if ( (runrc = system(state.override.run) != 0 ))
	     {
		  logerr("run script \"%s\" executed with non-zero return value", state.override.run);
	     }
	}
	
	setproctitle("bforce calling %.32s, %.32s",
		ftn_addrstr(abuf, state.node.addr), state.node.phone);
	
	if( opts->dontcall )
	{
		rc = call_system_quiet(opts->connect, opts->inetd);
	}
	else if( !inet )
	{
		if( (p_lockdir = conf_string(cf_uucp_lock_directory)) == NULL )
			p_lockdir = BFORCE_LOCK_DIR;
		
		if( opts->device && *opts->device )
		{
			if( (state.modemport =
					modem_getmatch_port(opts->device)) == NULL )
			{
				errmsg = "unknown port name";
				gotoexit(BFERR_PORTBUSY);
			}
		}
		else if( (state.modemport =
				modem_getfree_port(p_lockdir)) == NULL )
		{
			errmsg = "no free matching ports";
			gotoexit(BFERR_PORTBUSY);
		}
		
		if( port_lock(p_lockdir, state.modemport) )
		{
			errmsg = "cannot lock modem port";
			gotoexit(BFERR_PORTBUSY);
		}
		else /* Successfuly locked port */
		{
			rc = call_system_modem();
			port_unlock(p_lockdir, state.modemport);
		}
	}
	else
	{
		rc = call_system_tcpip();
	}

exit:
	out_bsy_unlockall();

	if( errmsg )
		log("call to %s failed: %s", ftn_addrstr(abuf, addr), errmsg);
	
	log("session rc = %d (\"%s\")", rc, BFERR_NAME(rc));
	
	(void)session_stat_update(&state.node.addr,
			&state.sess_stat, TRUE, rc);
	
	deinit_state(&state);
	return(rc);
}
