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
#include "outbound.h"
#include "daemon.h"

static s_modemport *daemon_getfree_port(const char *lockdir)
{
	s_cval_entry *ptrl;
		
	/* Find first not locked modem device */
	for( ptrl = conf_first(cf_modem_port); ptrl; ptrl = conf_next(ptrl) )
	{
		if( port_checklock(lockdir, &ptrl->d.modemport) == LOCKCHECK_NOLOCK
		 && modem_candialout(ptrl->d.modemport.name) == TRUE
		 && daemon_line_isready(ptrl->d.modemport.name) )
			return &ptrl->d.modemport;
	}
	
	return NULL;
}

static int daemon_call_branch(s_sysentry *syst, const char *lockdir, s_modemport *port)
{
	int rc = BFERR_NOERROR;
	char abuf[BF_MAXADDRSTR+1];

	/*
	 * First of all restore the default signal handlers
	 */
	signal(SIGCHLD, SIG_DFL);
	signal(SIGINT,  SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGHUP,  SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);

	/*
	 * Initialise ``state'' information structure
	 */
	init_state(&state);
	state.caller    = TRUE;
	state.valid     = TRUE;
	state.node      = syst->node;
	state.listed    = syst->node.listed;
	state.modemport = (!syst->tcpip) ? port : NULL;
	if( syst->lineptr )
		state.override = *syst->lineptr;
	if( *state.node.addr.domain )
		*state.node.addr.domain = '\0';

	/*
	 * Apply overrides to the node information
	 */	
	if( state.override.sFlags )
	{
		strnxcat(state.node.flags, ",", sizeof(state.node.flags));
		strnxcat(state.node.flags, state.override.sFlags, sizeof(state.node.flags));
	}

	if( !syst->tcpip && state.override.sPhone )
		(void)strnxcpy(state.node.phone, state.override.sPhone, sizeof(state.node.phone));
	else if( syst->tcpip && state.override.sIpaddr )
		(void)strnxcpy(state.node.phone, state.override.sIpaddr, sizeof(state.node.phone));

	/*
	 * Try to lock address of system we are going to call
	 */
#ifdef BFORCE_USE_CSY
	if( out_bsy_lock(state.node.addr, TRUE) )
#else
	if( out_bsy_lock(state.node.addr) )
#endif
		gotoexit(BFERR_SYSTEM_LOCKED);
	
	setproctitle("bforce calling %s, %s",
		ftn_addrstr(abuf, state.node.addr), state.node.phone);
	
	if( syst->tcpip )
	{
		rc = call_system_tcpip();
	}
	else /* via Modem */
	{
		state.modemport = port;
		if( port_lock(lockdir, state.modemport) )
		{
			log("cannot lock modem port");
			rc = BFERR_PORTBUSY;
		}
		else /* Locked port */
		{
			rc = call_system_modem();
			port_unlock(lockdir, state.modemport);
		}
	}

exit:
	out_bsy_unlockall();

	log("session rc = %d (\"%s\")", rc, BFERR_NAME(rc));

	(void)session_stat_update(&state.node.addr,
			&state.sess_stat, TRUE, rc);

	deinit_state(&state);
		
	return rc;
}

int daemon_call(s_sysentry *syst)
{
	pid_t chld_pid;
	const char *p_lockdir = NULL;
	char abuf[BF_MAXADDRSTR+1];
	s_modemport *modemport = NULL;
	
	/*
	 * Check number of allowed clients
	 */
	if( (syst->tcpip == TRUE  && max_tcpip && max_tcpip <= tcpip_clients)
	 || (syst->tcpip == FALSE && max_modem && max_modem <= modem_clients) )
	{
		DEB((D_DAEMON, "daemon_call: too many clients!"));
		return 1;
	}
	
	/*
	 * Check whether this node is allready locked
	 */
	if( out_bsy_check(syst->node.addr) )
		return 0;
	
	/*
	 * Set state structure to make expressions works properly now
	 */
	state.caller  = TRUE;
	state.valid   = TRUE;
	state.node    = syst->node;
	state.listed  = syst->node.listed;
	state.inet    = syst->tcpip;
	
	if( syst->tcpip == FALSE )
	{
		if( (p_lockdir = conf_string(cf_uucp_lock_directory)) == NULL )
			p_lockdir = BFORCE_LOCK_DIR;
		
		if( (modemport = daemon_getfree_port(p_lockdir)) == NULL )
		{
			init_state(&state);
			return 1;
		}
	}
	
	log("call %s line %d via %s",
		ftn_addrstr(abuf, syst->node.addr), syst->line,
		syst->tcpip ? "TCP/IP" : modemport->name);
	
	chld_pid = fork();

	if( chld_pid > 0 )
	{
		(void)daemon_branch_add(syst->node.addr, chld_pid, syst->tcpip,
				syst->tcpip ? NULL : modemport->name);
		
		init_state(&state);
		
		if( syst->tcpip )
			++tcpip_clients;
		else
			++modem_clients;
		
		return 0;
	}
	else if( chld_pid < 0 )
	{
		logerr("failed fork() call");
		return 1;
	}
	
	/* Now we are in child process */
	
	exit(daemon_call_branch(syst, p_lockdir, modemport));
}

