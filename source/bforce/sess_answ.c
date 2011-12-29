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

int answ_system(e_session type, char *connstr, int inetd)
{
	TIO oldtio;
	struct sockaddr_in client;
	int clientlen = sizeof(client);
	int rc = 0;
	char *p;
	
	init_state(&state);
	state.session  = type;
	state.caller   = FALSE;
	state.valid    = TRUE;

	/*
	 * Set verbal line name
	 */
	if( inetd )
	{
		state.linename = xstrcpy("tcpip");
		state.inet = TRUE;
	}
	else
		state.linename = isatty(0) ? port_get_name(ttyname(0)) : "tcpip";
	
	if( !inetd )
	{
		if( tio_get_dcd(0) == 0 )
			log("warning: DCD line is not active");
		
		if( (p = getenv("CALLER_ID")) && *p && strcmp(p, "none") )
			state.cidstr = (char*)xstrcpy(p);
		
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
		gotoexit(BFERR_FATALERROR);
	}

#ifdef DEBUG
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	if( inetd )
	{
		if( connstr && *connstr )
			state.connstr = (char*)xstrcpy(connstr);
		else if( getpeername(0, (struct sockaddr*)&client, &clientlen) == -1 )
			logerr("can't get client address");
		else
		{
#ifdef IPV6
			>char addr_str[INET6_ADDRSTRLEN+1];
			state.peername = (char*)xstrcpy(inet_ntop(AF_INET6, client.sin6_addr, addr_str, INET6_ADDRSTRLEN));
			state.peerport = (long)ntohs(client.sin6_port);
#else
			state.peername = (char*)xstrcpy(inet_ntoa(client.sin_addr));
			state.peerport = (long)ntohs(client.sin_port);
#endif
		}
	}
	
	if( inetd == 0 && state.cidstr )
	{
		setproctitle("bforce answering, CID: %.32s",
			string_printable(state.cidstr));
		log("Caller-ID: \"%s\"",
			string_printable(state.cidstr));
	}
	else if( inetd && state.peername )
	{
		setproctitle("bforce answering, host %.32s:%ld",
			string_printable(state.peername), state.peerport);
		log("TCP/IP connect from %s on port %ld",
			string_printable(state.peername), state.peerport);
	}
	else
	{
		setproctitle("bforce answering");
	}
	
	if( state.connstr )
		log("connect \"%s\" (%ld)", state.connstr, state.connspeed);

	if( (inetd == 0 && (rc = port_init(0, 0, &oldtio, FALSE)) == 0)
	 || (inetd == 1 && (rc = tcpip_init()) == 0) )
	{
		port_carrier(0, TRUE);
		
		rc = session();
		
		if( (inetd != 0) && (state.inet = FALSE) )
		{
			port_deinit(0, &oldtio);
			port_close();
		}
	}
	else
		rc = BFERR_FATALERROR;

exit:	
	out_bsy_unlockall();
	
	log("session rc = %d (\"%s\")", rc, BFERR_NAME(rc));
	
	if( state.node.addr.zone > 0 )
		(void)session_stat_update(&state.node.addr,
				&state.sess_stat, FALSE, rc);
	
	deinit_state(&state);
	return(rc);
}
