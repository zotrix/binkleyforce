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

#define CALL_STDIO		(1)
#define CALL_MODEM		(2)
#define CALL_TCPIP_BINKP	(4)
#define CALL_TCPIP_IFCICO	(8)
#define CALL_TCPIP_TELNET	(16)
#define CALL_TCPIP_ANY		(CALL_TCPIP_BINKP | CALL_TCPIP_IFCICO | CALL_TCPIP_TELNET)

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
#ifdef IPV6
			char addr_str[INET6_ADDRSTRLEN+1];
			state.peername = (char*)xstrcpy(inet_ntop(AF_INET6, client.sin6_addr, addr_str, INET6_ADDRSTRLEN));
			state.peerport = (long)ntohs(client.sin6_port);
#else
			state.peername = (char*)xstrcpy(inet_ntoa(client.sin_addr));
			state.peerport = (long)ntohs(client.sin_port);
#endif
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

	// automatically determine session type
	state.session = SESSION_UNKNOWN;

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

int call_system_tcpip(int callwith) // only TCPIP values
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
	
	switch( callwith ) {
case CALL_TCPIP_BINKP:
		state.tcpmode = TCPMODE_BINKP;
		state.session = SESSION_BINKP;
		break;
case CALL_TCPIP_IFCICO:
		state.tcpmode = TCPMODE_RAW;
		state.session = SESSION_UNKNOWN;
		break;
case CALL_TCPIP_TELNET:
		state.tcpmode = TCPMODE_TELNET;
		state.session = SESSION_UNKNOWN;
		break;
defalt:
		log("invalid protocol for TCP/IP module");
		return BFERR_FATALERROR;
	}

	log("calling %s (%s, %s)",
		ftn_addrstr(abuf, state.node.addr),
		(state.node.name  && *state.node.name ) ? state.node.name  : "<none>",
		(state.node.host && *state.node.host) ? state.node.host : "<none>");
		
	if( (rc = tcpip_connect(state.node.host, state.tcpmode)) == 0
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
	// if added new opts check daemon_call
	
	char s[300];
	snprintf(s, 299, "bforce calling system %d:%d/%d.%d", addr.zone, addr.net, addr.node, addr.point );
	log(s);
	
	// find suitable way of connection and try to make session
	
        int rc = 0;
        int runrc = 0;
	char abuf[BF_MAXADDRSTR+1];
	char *p_lockdir = NULL;
	char *errmsg = NULL;
	int call_mustuse = 0;
	int call_mayuse = 0;

	init_state(&state);

	state.caller    = TRUE;
	state.valid     = TRUE;
	state.node.addr = addr;
	
	nodelist_lookup(&state.node, addr);
	if( override_get(&state.override, state.node.addr, opts->hiddline) )
	{
		errmsg = "incorrect hidden line number";
		gotoexit(BFERR_PHONE_UNKNOWN);
	}
	
	state.listed    = state.node.listed;
	state.node.addr.domain[0] = '\0'; /* Discard domain for node address */

	// 1. If call method specified in cmdline, do use it
	// 2. If not, use nodelist data and overrides and call all available methods
	//    If override contains Phone or IP flags, ignore nodelist connect methods (but save INA if not overrided)

	// 1st - get all allowed call ways
	// 2nd - gather information reqired to call and remove unavailable ways (no info, node does not support)

	call_mayuse = 0;

	if( opts->runmode == MODE_CALL_DEFAULT )
	{
		call_mayuse = CALL_MODEM | CALL_TCPIP_ANY;
	}
	else if( opts->runmode == MODE_CALL_STDIO )
	{
		call_mustuse = CALL_STDIO;
	}
	else if( opts->runmode == MODE_CALL_MODEM )
	{
		call_mustuse = CALL_MODEM;
	}
	else if( opts->runmode == MODE_CALL_IP )
	{
		if( strcasecmp(opts->ipproto, "binkp") == 0 )
		{
			call_mustuse = CALL_TCPIP_BINKP;
		}
		else if ( strcasecmp(opts->ipproto, "ifcico") == 0 )
		{
			call_mustuse = CALL_TCPIP_IFCICO;
		}
		else if( strcasecmp(opts->ipproto, "telnet") == 0 )
		{
			call_mustuse = CALL_TCPIP_TELNET;
		}
		else if( opts->ipproto == NULL ) // determine from nodelist/override
		{
			call_mayuse = CALL_TCPIP_ANY;
			call_mustuse = 0;
		}
		else
		{
			log("Unknown protocol");
			return -1;
		}
	}
	else
	{
		log("Unknown protocol");
		return -1;
	}
	
	call_mayuse |= call_mustuse; // it simplifies logics: all required is allowed

       //char s[300];
       //snprintf(s, 299, "initial: may use %d must use %d", call_mayuse, call_mustuse);
       //log(s);


	if( call_mayuse & CALL_MODEM )
	{
		// 1. use phone from opts
		// 2. use overrides
		// 3. use nodelist
		
		if( opts->phone && *opts->phone )
		{
			(void)strnxcpy(state.node.phone, opts->phone, sizeof(state.node.phone));
			//log("phone from options");
		}
		else if( state.override.sPhone )
		{
			(void)strnxcpy(state.node.phone, state.override.sPhone, sizeof(state.node.phone));
			//log("phone from override");
		}
	
		if( !modem_isgood_phone(state.node.phone) )
		{
			log("bad phone, excluding modem");
			call_mayuse &= ~CALL_MODEM;
			if( call_mustuse & CALL_MODEM )
			{
				errmsg = "don't know phone number";
				gotoexit(BFERR_PHONE_UNKNOWN);
			}
		}
	/*
	 * Is now a working time for that node/line
	 */
		if( !opts->force)
		{
			time_t unixtime = time(NULL);
			struct tm *now = localtime(&unixtime);
			bool goodtime = FALSE;
		
			if( timevec_isdefined(&state.override.worktime) )
			{
				goodtime = timevec_isnow(&state.override.worktime, now);
			}
			else if( state.override.sFlags )
			{
				goodtime = !nodelist_checkflag(state.override.sFlags, "CM");
			}
			else if( !opts->hiddline )
			{
				if( !nodelist_checkflag(state.node.flags, "CM") )
					goodtime = TRUE;
				else
					goodtime = timevec_isnow(&state.node.worktime, now);
			}
			if( !goodtime )
			{
				call_mayuse &= ~CALL_MODEM;
				log("bad worktime, excluding modem");
				if( call_mustuse & CALL_MODEM )
				{
					errmsg = "not works now, try later";
					gotoexit(BFERR_NOTWORKING);
				}
			}
		}
	}

//       snprintf(s, 299, "after phone check: may use %d must use %d", call_mayuse, call_mustuse);
//       log(s);

	/*
	 * Apply overrides to the node information
	 */	
	
	if( state.override.sFlags )
	{
		strnxcat(state.node.flags, ",", sizeof(state.node.flags));
		strnxcat(state.node.flags, state.override.sFlags, sizeof(state.node.flags));
	}
	
	// state.node		nodelist
	// state.override	config
	// opts			cmdline
	
	// filter unavailable protos if not obligated to use it
	
	if( !(call_mustuse & CALL_TCPIP_BINKP) && (call_mayuse & CALL_TCPIP_BINKP) )
	{
		if( nodelist_checkflag(state.node.flags, "BINKP") != 0 && nodelist_checkflag(state.node.flags, "IBN") != 0 )
		{
			call_mayuse &= ~CALL_TCPIP_BINKP;
		}
	}

	if( !(call_mustuse & CALL_TCPIP_IFCICO) && (call_mayuse & CALL_TCPIP_IFCICO) )
	{
		if( nodelist_checkflag(state.node.flags, "IFC") != 0 && nodelist_checkflag(state.node.flags, "IFC") != 0 )
		{
			call_mayuse &= ~CALL_TCPIP_IFCICO;
		}
	}

	if( !(call_mustuse & CALL_TCPIP_TELNET) && (call_mayuse & CALL_TCPIP_TELNET) )
	{
		if( nodelist_checkflag(state.node.flags, "TELN") != 0 && nodelist_checkflag(state.node.flags, "TLN") != 0 )
		{
			call_mayuse &= ~CALL_TCPIP_TELNET;
		}
	}

	if( opts->iphost && *opts->iphost )
	{
		(void)strnxcpy(state.node.host, opts->iphost, sizeof(state.node.host));
	}
	else if( state.override.sIpaddr )
	{
		(void)strnxcpy(state.node.host,
				state.override.sIpaddr, sizeof(state.node.host));
	}
	
	if( call_mayuse & CALL_TCPIP_ANY && !tcpip_isgood_host(state.node.host) )
	{
		call_mayuse &= ~CALL_TCPIP_ANY;
		log("bad host, exclude IP");
		if( call_mustuse & CALL_TCPIP_ANY )
		{
			errmsg = "don't know host name";
			gotoexit(BFERR_PHONE_UNKNOWN);
		}
	}

//       snprintf(s, 299, "after IP check: may use %d must use %d", call_mayuse, call_mustuse);
//       log(s);
	
//do_session:
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
	
	
	// try allowed methods and break if rc == 0
	rc = -1;

	if( rc && call_mayuse & CALL_STDIO )
	{
		rc = call_system_quiet(opts->connect, opts->inetd);
	}

	if( rc && call_mayuse & CALL_TCPIP_BINKP )
	{
		rc = call_system_tcpip(CALL_TCPIP_BINKP);
	}

	if( rc && call_mayuse & CALL_TCPIP_IFCICO )
	{
		rc = call_system_tcpip(CALL_TCPIP_IFCICO);
	}

	if( rc && call_mayuse & CALL_TCPIP_TELNET )
	{
		rc = call_system_tcpip(CALL_TCPIP_TELNET);
	}

	if( rc && call_mayuse & CALL_MODEM )
	{
		setproctitle("bforce calling %.32s, %.32s",
			ftn_addrstr(abuf, state.node.addr), state.node.phone);
		rc = -1;
		if( (p_lockdir = conf_string(cf_uucp_lock_directory)) == NULL )
			p_lockdir = BFORCE_LOCK_DIR;
		
		if( opts->device && *opts->device )
		{
			state.modemport = modem_getmatch_port(opts->device);
		}
		else
		{
			state.modemport = modem_getfree_port(p_lockdir);
		}
		
		if( state.modemport )
		{
		
		    if( port_lock(p_lockdir, state.modemport) == 0 ) /* Successfuly locked port */
		    {
		    	rc = call_system_modem();
		    	port_unlock(p_lockdir, state.modemport);
		    }
		    else
		    {
			errmsg = "cannot lock modem port";
		    }
		}
		else
		{
		    errmsg = "unable to get modem port";
		}
	}

	if( rc )
	{
	    log("no connection effort was successful");
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
