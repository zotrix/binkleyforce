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
#include "session.h"

/*
 * Environment variables:
 *   $REM_ADDR_FTN,
 *   $REM_ADDR_INET,
 *   $LOC_ADDR_FTN,
 *   $LOC_ADDR_INET,
 *   $PASSWORD,
 *   $MAILER,
 *   $LOCATION,
 *   $SYSOP,
 *   $SYSTEM_NAME
 *   $PHONE,
 *   $FLAGS
 *   $PROTECTED,
 *   $LISTED,
 *   $INBOUND
 *   $CONNECT,
 *   $CALLERID,
 *   $RC,
 *   $PEERNAME
 */
int session_run_command(const char *execstr)
{
	int rc = 0;
	s_faddr *addr;
	char abuf[BF_MAXADDRSTR+1];
	char sbuf[32];
	s_exec_options eopts;
	char *p;
	
	exec_options_init(&eopts);
	
	if( state.handshake )
	{
		if( addr = session_1remote_address() )
		{
			exec_env_add(&eopts, "REM_ADDR_FTN",
					ftn_addrstr_fido(abuf, *addr));
			exec_env_add(&eopts, "REM_ADDR_INET",
					ftn_addrstr_inet(abuf, *addr));
		}
	
		if( addr = session_1local_address() )
		{
			exec_env_add(&eopts, "LOC_ADDR_FTN",
					ftn_addrstr_fido(abuf, *addr));
			exec_env_add(&eopts, "LOC_ADDR_INET",
					ftn_addrstr_inet(abuf, *addr));
		}
		
		if( state.handshake->remote_password
		 && (p = state.handshake->remote_password(state.handshake)) )
			exec_env_add(&eopts, "PASSWORD", p);
		
		if( state.handshake->remote_mailer
		 && (p = state.handshake->remote_mailer(state.handshake)) )
			exec_env_add(&eopts, "MAILER", p);
		
		if( state.handshake->remote_location
		 && (p = state.handshake->remote_location(state.handshake)) )
			exec_env_add(&eopts, "LOCATION", p);

		if( state.handshake->remote_sysop_name
		 && (p = state.handshake->remote_sysop_name(state.handshake)) )
			exec_env_add(&eopts, "SYSOP", p);
		
		if( state.handshake->remote_system_name
		 && (p = state.handshake->remote_system_name(state.handshake)) )
			exec_env_add(&eopts, "SYSTEM_NAME", p);
		
		if( state.handshake->remote_phone
		 && (p = state.handshake->remote_phone(state.handshake)) )
			exec_env_add(&eopts, "PHONE", p);

		if( state.handshake->remote_flags
		 && (p = state.handshake->remote_flags(state.handshake)) )
			exec_env_add(&eopts, "FLAGS", p);
	}

	if( state.valid )
	{
		exec_env_add(&eopts, "PROTECTED", state.protected ? "1" : "0");
		exec_env_add(&eopts, "LISTED", state.listed ? "1" : "0");

		if( state.inbound && *state.inbound )
			exec_env_add(&eopts, "INBOUND", state.inbound);
	
		if( state.connstr && *state.connstr )
			exec_env_add(&eopts, "CONNECT", state.connstr);
	
		if( state.cidstr && *state.cidstr )
			exec_env_add(&eopts, "CALLERID", state.cidstr);
			
		if( state.peername && *state.peername )
			exec_env_add(&eopts, "PEERNAME", state.peername);
				
		
		if( state.session_rc >= 0 )
		{
			sprintf(sbuf, "%d", state.session_rc);
			exec_env_add(&eopts, "RC", sbuf);
		}
	}
	
	exec_options_set_command(&eopts, execstr);
	
	rc = exec_command(&eopts);
	
	exec_options_deinit(&eopts);
	
	return rc;
}

int override_get(s_override *dest, s_faddr addr, int line)
{
	s_override *p;
	int curline = 0;
	
	p = conf_override(cf_override, addr);
	
	curline = 0;
	while( p && curline < line )
	{
		p = p->hidden;
		++curline;
	}
	
	if( p && curline == line )
	{
		*dest = *p;
		return 0;
	}
	else if( line == 0 )
		return 0;
	else
		return 1;
}

void init_state(s_state *pstate)
{
	memset(pstate, '\0', sizeof(s_state));
	pstate->session_rc = -1;
}

void deinit_state(s_state *pstate)
{
        DEB((D_FREE, "deinit_state begin"));

        DEB((D_FREE, "deinit_state linename"));
	if (pstate->linename) free(pstate->linename);
        DEB((D_FREE, "deinit_state cidstr"));
	if (pstate->cidstr) free(pstate->cidstr);
        DEB((D_FREE, "deinit_state peername"));
	if (pstate->peername) free(pstate->peername);
        DEB((D_FREE, "deinit_state connstr"));
	if (pstate->connstr) free(pstate->connstr);
        DEB((D_FREE, "deinit_state inbound"));
	if (pstate->inbound) free(pstate->inbound);
        DEB((D_FREE, "deinit_state tinbound"));
	if (pstate->tinbound) free(pstate->tinbound);
        DEB((D_FREE, "deinit_state mailfor"));
	if (pstate->mailfor) deinit_falist(pstate->mailfor);

        DEB((D_FREE, "deinit_state fsqueue"));
	deinit_fsqueue(&pstate->queue);

        DEB((D_FREE, "deinit_state handshake"));
	if (state.handshake && state.handshake->deinit) {
		state.handshake->deinit(state.handshake);
	}

        DEB((D_FREE, "deinit_state remotedata"));
	if (pstate->remoteaddrs) free (pstate->remoteaddrs);
        DEB((D_FREE, "deinit_state localdata"));
	if (pstate->localaddrs) free (pstate->localaddrs);
	
	memset(pstate, '\0', sizeof(s_state));

	pstate->session_rc = -1;
        DEB((D_FREE, "deinit_state end"));
}

s_faddr *session_1remote_address()
{
	if (state.n_remoteaddr > 0) return &state.remoteaddrs[0].addr;

	return NULL;
}

s_faddr *session_1local_address()
{
	if (state.n_localaddr > 0) return &state.localaddrs[0].addr;

	return NULL;
}
