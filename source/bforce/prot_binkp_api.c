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
#include "nodelist.h"
#include "session.h"
#include "prot_binkp.h"

void binkp_init(s_handshake_protocol *THIS);
void binkp_deinit(s_handshake_protocol *THIS);
int binkp_incoming2(s_handshake_protocol *THIS);
int binkp_outgoing2(s_handshake_protocol *THIS);
s_faddr *binkp_remote_address(s_handshake_protocol *THIS);
char *binkp_remote_password(s_handshake_protocol *THIS);
char *binkp_remote_sysop_name(s_handshake_protocol *THIS);
char *binkp_remote_system_name(s_handshake_protocol *THIS);
char *binkp_remote_location(s_handshake_protocol *THIS);
char *binkp_remote_phone(s_handshake_protocol *THIS);
char *binkp_remote_flags(s_handshake_protocol *THIS);
char *binkp_remote_mailer(s_handshake_protocol *THIS);
s_faddr *binkp_local_address(s_handshake_protocol *THIS);
char *binkp_local_password(s_handshake_protocol *THIS);

s_handshake_protocol handshake_protocol_binkp = {
	/* Section 1 */
	"BinkP",
	"",
	"",
	NULL,
	NULL,
	0,
	binkp_init,
	binkp_deinit,
	binkp_incoming2,
	binkp_outgoing2,
	/* Section 2 */
//	binkp_remote_address,
	binkp_remote_password,
	binkp_remote_sysop_name,
	binkp_remote_system_name,
	binkp_remote_location,
	binkp_remote_phone,
	binkp_remote_flags,
	binkp_remote_mailer,
	NULL,
	/* Section 3 */
//	binkp_local_address,
	binkp_local_password
};

void binkp_init(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data == NULL);
	ASSERT(THIS->local_data == NULL);

	THIS->remote_data = (char *)xmalloc(sizeof(s_binkp_sysinfo));
	THIS->local_data = (char *)xmalloc(sizeof(s_binkp_sysinfo));
	THIS->protocol = PROT_BINKP;

	memset(THIS->remote_data, '\0', sizeof(s_binkp_sysinfo));
	memset(THIS->local_data, '\0', sizeof(s_binkp_sysinfo));
}

void binkp_deinit(s_handshake_protocol *THIS)
{
        log("binkp_deinit");
	if (THIS==NULL) {
	    log("THIS==NULL");
	    return;
	}

	if( THIS->remote_data )
	{
		memset(THIS->remote_data, '\0', sizeof(s_binkp_sysinfo));
		free(THIS->remote_data);
		THIS->remote_data = NULL;
	}

	if( THIS->local_data )
	{
		memset(THIS->local_data, '\0', sizeof(s_binkp_sysinfo));
		free(THIS->local_data);
		THIS->local_data = NULL;
	}
        log("binkp_deinit end");

}

int binkp_incoming2(s_handshake_protocol *THIS)
{
	int rc = -1;
	s_binkp_sysinfo *remote_data = NULL;
	s_binkp_sysinfo *local_data = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);
	
	remote_data = (s_binkp_sysinfo *)THIS->remote_data;
	local_data = (s_binkp_sysinfo *)THIS->local_data;
	
	binkp_set_sysinfo(local_data, NULL, FALSE);

	rc = binkp_incoming(local_data, remote_data);

	binkp_log_sysinfo(remote_data);
	if( state.n_remoteaddr > 0 )
	{
		session_remote_lookup(state.remoteaddrs, state.n_remoteaddr);
		session_remote_log_status();
		binkp_log_options(remote_data);
	}

	if( rc == HRC_OK )
	{
		/*
		 * Create mail/files queue
		 */
		session_create_files_queue(state.remoteaddrs,
	    	                       state.n_remoteaddr);
		session_set_send_options();
		session_set_inbound();
		session_set_freqs_status();
	}
	
	return rc;
}

int binkp_outgoing2(s_handshake_protocol *THIS)
{
	int rc = HRC_OTHER_ERR;
	s_binkp_sysinfo *remote_data = NULL;
	s_binkp_sysinfo *local_data = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);
	
	remote_data = (s_binkp_sysinfo *)THIS->remote_data;
	local_data = (s_binkp_sysinfo *)THIS->local_data;

	session_set_freqs_status();
	
	binkp_set_sysinfo(local_data, &state.node.addr, TRUE);
	
	rc = binkp_outgoing(local_data, remote_data);

	binkp_log_sysinfo(remote_data);
	if( state.n_remoteaddr > 0 )
	{
		session_remote_lookup(state.remoteaddrs, state.n_remoteaddr);
		session_remote_log_status();
		binkp_log_options(remote_data);
	}

	if( rc == HRC_OK )
	{
		/*
		 * Create mail/files queue
		 */
		session_create_files_queue(state.remoteaddrs, state.n_remoteaddr);
		session_set_send_options();
		session_set_inbound();
	}
	
	return rc;
}

/*s_faddr *binkp_remote_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->anum > 0 )
		return &((s_binkp_sysinfo *)THIS->remote_data)->addrs[0].addr;

	return NULL;
} */

char *binkp_remote_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->passwd[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->passwd;

	return NULL;
}

char *binkp_remote_sysop_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->sysop[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->sysop;

	return NULL;
}

char *binkp_remote_system_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->systname[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->systname;

	return NULL;
}

char *binkp_remote_location(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->location[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->location;

	return NULL;
}

char *binkp_remote_phone(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->phone[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->phone;

	return NULL;
}

char *binkp_remote_flags(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->flags[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->flags;

	return NULL;
}

char *binkp_remote_mailer(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_binkp_sysinfo *)THIS->remote_data)->progname[0] )
		return ((s_binkp_sysinfo *)THIS->remote_data)->progname;

	return NULL;
}

/*s_faddr *binkp_local_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);

	if( ((s_binkp_sysinfo *)THIS->local_data)->anum > 0 )
		return &((s_binkp_sysinfo *)THIS->local_data)->addrs[0].addr;

	return NULL;
} */

char *binkp_local_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);
	
	if( ((s_binkp_sysinfo *)THIS->local_data)->passwd[0] )
		return ((s_binkp_sysinfo *)THIS->local_data)->passwd;
	
	return NULL;
}

