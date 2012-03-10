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
#include "prot_emsi.h"

void emsi_init(s_handshake_protocol *THIS);
void emsi_deinit(s_handshake_protocol *THIS);
int emsi_incoming(s_handshake_protocol *THIS);
int emsi_outgoing(s_handshake_protocol *THIS);
//s_faddr *emsi_remote_address(s_handshake_protocol *THIS);
char *emsi_remote_password(s_handshake_protocol *THIS);
char *emsi_remote_sysop_name(s_handshake_protocol *THIS);
char *emsi_remote_system_name(s_handshake_protocol *THIS);
char *emsi_remote_location(s_handshake_protocol *THIS);
char *emsi_remote_phone(s_handshake_protocol *THIS);
char *emsi_remote_flags(s_handshake_protocol *THIS);
char *emsi_remote_mailer(s_handshake_protocol *THIS);
int  emsi_remote_traffic(s_handshake_protocol *THIS, s_traffic *dest);
//s_faddr *emsi_local_address(s_handshake_protocol *THIS);
char *emsi_local_password(s_handshake_protocol *THIS);

s_handshake_protocol handshake_protocol_emsi = {
	/* Section 1 */
	"EMSI",
	"Joaquim H. Homrighausen",
	"FSC-0056,0088",
	NULL,
	NULL,
	0,
	emsi_init,
	emsi_deinit,
	emsi_incoming,
	emsi_outgoing,
	/* Section 2 */
//	emsi_remote_address,
	emsi_remote_password,
	emsi_remote_sysop_name,
	emsi_remote_system_name,
	emsi_remote_location,
	emsi_remote_phone,
	emsi_remote_flags,
	emsi_remote_mailer,
	emsi_remote_traffic,
	/* Section 3 */
//	emsi_local_address,
	emsi_local_password
};

static void emsi_set_send_options(s_emsi *remote_emsi)
{
	session_set_send_options();

	if( remote_emsi->linkcodes.FNC )
		state.sopts.fnc = 1;
	
	if( remote_emsi->linkcodes.RH1 )
		state.sopts.hydraRH1 = 1;

	if( state.caller )
	{
		if( remote_emsi->linkcodes.HRQ
		 || remote_emsi->compcodes.NRQ )
			state.sopts.holdreq = 1;
		if( remote_emsi->linkcodes.HAT )
			state.sopts.holdall = 1;
		if( remote_emsi->linkcodes.HXT )
			state.sopts.holdxt = 1;
	}
	else
	{
		if( remote_emsi->linkcodes.NPU )
			state.sopts.holdall = 1;
		if( remote_emsi->linkcodes.PMO )
			state.sopts.holdfiles = 1;
		if( remote_emsi->linkcodes.NRQ )
			state.sopts.holdreq = 1;
	}
}

void emsi_init(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data == NULL);
	ASSERT(THIS->local_data == NULL);

	THIS->remote_data = (char *)xmalloc(sizeof(s_emsi));
	THIS->local_data = (char *)xmalloc(sizeof(s_emsi));

	memset(THIS->remote_data, '\0', sizeof(s_emsi));
	memset(THIS->local_data, '\0', sizeof(s_emsi));
}

void emsi_deinit(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);

	if( THIS->remote_data )
	{
		memset(THIS->remote_data, '\0', sizeof(s_emsi));
		free(THIS->remote_data);
	}

	if( THIS->local_data )
	{
		memset(THIS->local_data, '\0', sizeof(s_emsi));
		free(THIS->local_data);
	}
}

/*
 * Do incoming emsi session
 */
int emsi_incoming(s_handshake_protocol *THIS)
{
	int rc = HRC_OK;
	s_emsi *remote_emsi = NULL;
	s_emsi *local_emsi = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);
	
	remote_emsi = (s_emsi *)THIS->remote_data;
	local_emsi = (s_emsi *)THIS->local_data;
	
	/*
	 * Receive EMSI_DAT packet
	 */
	if( emsi_recv_emsidat(remote_emsi) )
		return HRC_OTHER_ERR;

	/*
	 * Check password(s)
	 */
	if( session_addrs_check(state.remoteaddrs, state.n_remoteaddr,
				remote_emsi->passwd, NULL, 0) )
	{
		rc = HRC_BAD_PASSWD;
		/* Don't return. Send EMSI_DAT with
		 * invalid password messages */
	}
	else
	{
		/* Lock (create BSY) remote addresses */
		if( session_addrs_lock(state.remoteaddrs, state.n_remoteaddr) )
		{
			log("all remote addresses are busy");
			rc = HRC_BUSY;
		}
		else
		{
			/*
			 * We know caller's address so we can process
			 * more expressions and fill state.node structure..
			 */
			session_remote_lookup(state.remoteaddrs, state.n_remoteaddr);
			
			if( session_check_speed() )
				rc = HRC_LOW_SPEED;
		}
	}

	/*
	 * Put EMSI information to log
	 */
	(void)emsi_logdat(remote_emsi);

	session_remote_log_status();

	if( rc == HRC_OK )
	{
		const long options = conf_options(cf_options);
		
		/*
		 * Set inbound directories, ignore errors,
		 * because we want send mail in any case.
		 */
		(void)session_set_inbound();
				
		/*
		 * Set protocol that we will use
		 */
		if( remote_emsi->compcodes.HYD && !(options & OPTIONS_NO_HYDRA) )
			THIS->protocol = PROT_HYDRA;
		else if( remote_emsi->compcodes.ZAP && !(options & OPTIONS_NO_ZEDZAP) )
			THIS->protocol = PROT_ZEDZAP;
		else if( remote_emsi->compcodes.ZMO && !(options & OPTIONS_NO_ZMODEM) )
			THIS->protocol = PROT_ZMODEM;
		else /* NCP */
		{
			THIS->protocol = PROT_NOPROT;
			rc = HRC_NO_PROTOS;
		}
		
		/*
		 * Check EMSI flags and set corresponding local HOLD flags
		 */
		(void)emsi_set_send_options(remote_emsi);
		
		/*
		 * Create mail/files queue
		 */
		session_create_files_queue(state.remoteaddrs,
		                           state.n_remoteaddr);
		
		/*
		 * Set FREQ processor status
		 */
		if( rc == HRC_OK && THIS->protocol != PROT_NOPROT )
			session_set_freqs_status();
		else
			state.reqstat = REQS_DISABLED;
	}
	
	/*
	 * Prepare ``local_emsi'' structure
	 */
	(void)emsi_set_sysinfo(local_emsi, remote_emsi, rc, THIS->protocol);
	
	/*
	 * Send EMSI_DAT packet
	 */
	if( emsi_send_emsidat(local_emsi) )
		return HRC_OTHER_ERR;
	
	return rc;
}

/*
 * Do outgoing emsi session
 */
int emsi_outgoing(s_handshake_protocol *THIS)
{
	s_emsi *remote_emsi = NULL;
	s_emsi *local_emsi = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);
	
	remote_emsi = (s_emsi *)THIS->remote_data;
	local_emsi = (s_emsi *)THIS->local_data;
	
	/*
	 * Set FREQ processor status
	 */
	session_set_freqs_status();

	/*
	 * Prepare ``local_emsi'' structure
	 */
	(void)emsi_set_sysinfo(local_emsi, NULL, HRC_OK, THIS->protocol);
	
	/*
	 * Send EMSI_DAT packet
	 */
	if( emsi_send_emsidat(local_emsi) )
		return HRC_OTHER_ERR;

	/*
	 * Receive EMSI_DAT packet
	 */
	if( emsi_recv_emsidat(remote_emsi) )
		return HRC_OTHER_ERR;
	
	/*
	 * Put EMSI information to the log
	 */
	(void)emsi_logdat(remote_emsi);
	
	/*
	 * Make sure expected address was presented
	 */
	if( session_addrs_check_genuine(state.remoteaddrs,
				state.n_remoteaddr, state.node.addr) )
		return HRC_NO_ADDRESS;
		
	/*
	 * Check password(s)
	 */
	if( session_addrs_check(state.remoteaddrs, state.n_remoteaddr,
				remote_emsi->passwd, NULL, 0) )
		return HRC_BAD_PASSWD;
	
	/*
	 * Lock (create BSY) remote addresses
	 */
	(void)session_addrs_lock(state.remoteaddrs, state.n_remoteaddr);
	
	/*
	 * Set protocol we will use ("options" ignored)
	 */
	if( remote_emsi->compcodes.HYD && local_emsi->compcodes.HYD )
		THIS->protocol = PROT_HYDRA;
	else if( remote_emsi->compcodes.JAN && local_emsi->compcodes.JAN )
		THIS->protocol = PROT_JANUS;
	else if( remote_emsi->compcodes.DZA && local_emsi->compcodes.DZA )
		THIS->protocol = PROT_DIRZAP;
	else if( remote_emsi->compcodes.ZAP && local_emsi->compcodes.ZAP )
		THIS->protocol = PROT_ZEDZAP;
	else if( remote_emsi->compcodes.ZMO && local_emsi->compcodes.ZMO )
		THIS->protocol = PROT_ZMODEM;
	else
		return HRC_NO_PROTOS;
	
	/*
	 * Show remote status (prot/unprot, listed/unlisted)
	 */
	session_remote_log_status();
	
	/*
	 * Set inbound directories
	 */
	if( session_set_inbound() )
		return HRC_OTHER_ERR;
	
	/*
	 * Check EMSI flags and set corresponding local HOLD flags
	 */
	(void)emsi_set_send_options(remote_emsi);

	/*
	 * Create mail/files queue
	 */
	session_create_files_queue(state.remoteaddrs, state.n_remoteaddr);
	
	return HRC_OK;
}

/*s_faddr *emsi_remote_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->anum > 0 )
		return &((s_emsi *)THIS->remote_data)->addrs[0].addr;

	return NULL;
} */

char *emsi_remote_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->passwd[0] )
		return ((s_emsi *)THIS->remote_data)->passwd;

	return NULL;
}

char *emsi_remote_sysop_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->sysop[0] )
		return ((s_emsi *)THIS->remote_data)->sysop;

	return NULL;
}

char *emsi_remote_system_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->sname[0] )
		return ((s_emsi *)THIS->remote_data)->sname;

	return NULL;
}

char *emsi_remote_location(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->location[0] )
		return ((s_emsi *)THIS->remote_data)->location;

	return NULL;
}

char *emsi_remote_phone(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->phone[0] )
		return ((s_emsi *)THIS->remote_data)->phone;

	return NULL;
}

char *emsi_remote_flags(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->flags[0] )
		return ((s_emsi *)THIS->remote_data)->flags;

	return NULL;
}

char *emsi_remote_mailer(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_emsi *)THIS->remote_data)->m_name[0] )
		return ((s_emsi *)THIS->remote_data)->m_name;

	return NULL;
}

int emsi_remote_traffic(s_handshake_protocol *THIS, s_traffic *dest)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(dest);
	
	memset(dest, '\0', sizeof(s_traffic));

	if( ((s_emsi *)THIS->remote_data)->have_traf
	 || ((s_emsi *)THIS->remote_data)->have_moh )
	{
		dest->netmail_size = ((s_emsi *)THIS->remote_data)->netmail_size;
		dest->arcmail_size = ((s_emsi *)THIS->remote_data)->arcmail_size;
		dest->files_size = ((s_emsi *)THIS->remote_data)->files_size;

		return 0;
	}

	return -1;
}

/*s_faddr *emsi_local_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);

	if( ((s_emsi *)THIS->local_data)->anum > 0 )
		return &((s_emsi *)THIS->local_data)->addrs[0].addr;

	return NULL;
} */

char *emsi_local_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);
	
	if( ((s_emsi *)THIS->local_data)->passwd[0] )
		return ((s_emsi *)THIS->local_data)->passwd;
	
	return NULL;
}

