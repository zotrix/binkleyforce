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
#include "prot_yoohoo.h"

void yoohoo_init(s_handshake_protocol *THIS);
void yoohoo_deinit(s_handshake_protocol *THIS);
int yoohoo_incoming(s_handshake_protocol *THIS);
int yoohoo_outgoing(s_handshake_protocol *THIS);
s_faddr *yoohoo_remote_address(s_handshake_protocol *THIS);
char *yoohoo_remote_password(s_handshake_protocol *THIS);
char *yoohoo_remote_sysop_name(s_handshake_protocol *THIS);
char *yoohoo_remote_system_name(s_handshake_protocol *THIS);
char *yoohoo_remote_location(s_handshake_protocol *THIS);
char *yoohoo_remote_phone(s_handshake_protocol *THIS);
char *yoohoo_remote_flags(s_handshake_protocol *THIS);
char *yoohoo_remote_mailer(s_handshake_protocol *THIS);
s_faddr *yoohoo_local_address(s_handshake_protocol *THIS);
char *yoohoo_local_password(s_handshake_protocol *THIS);

s_handshake_protocol handshake_protocol_yoohoo = {
	/* Section 1 */
	"YooHoo",
	"",
	"",
	NULL,
	NULL,
	0,
	yoohoo_init,
	yoohoo_deinit,
	yoohoo_incoming,
	yoohoo_outgoing,
	/* Section 2 */
	yoohoo_remote_address,
	yoohoo_remote_password,
	yoohoo_remote_sysop_name,
	yoohoo_remote_system_name,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	/* Section 3 */
	yoohoo_local_address,
	yoohoo_local_password
};

void yoohoo_init(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data == NULL);
	ASSERT(THIS->local_data == NULL);

	THIS->remote_data = (char *)xmalloc(sizeof(s_yoohoo_sysinfo));
	THIS->local_data = (char *)xmalloc(sizeof(s_yoohoo_sysinfo));

	memset(THIS->remote_data, '\0', sizeof(s_yoohoo_sysinfo));
	memset(THIS->local_data, '\0', sizeof(s_yoohoo_sysinfo));
}

void yoohoo_deinit(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);

	if( THIS->remote_data )
	{
		memset(THIS->remote_data, '\0', sizeof(s_yoohoo_sysinfo));
		free(THIS->remote_data);
	}

	if( THIS->local_data )
	{
		memset(THIS->local_data, '\0', sizeof(s_yoohoo_sysinfo));
		free(THIS->local_data);
	}
}

int yoohoo_incoming(s_handshake_protocol *THIS)
{
	int rc = HRC_OK;
	s_yoohoo_sysinfo *remote_data = NULL;
	s_yoohoo_sysinfo *local_data = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);

	remote_data = (s_yoohoo_sysinfo *)THIS->remote_data;
	local_data = (s_yoohoo_sysinfo *)THIS->local_data;
	
	if( yoohoo_recv_hello(remote_data) )
		return HRC_OTHER_ERR;
	
	/*
	 * Check password(s)
	 */
	if( session_addrs_check(remote_data->addrs, remote_data->anum,
	                        remote_data->passwd, NULL, 0) )
	{
		rc = HRC_BAD_PASSWD;
		/* Don't return. Send HELLO with
		 * invalid password messages */
	}
	else
	{
		/* Lock (create BSY) remote addresses */
		if( session_addrs_lock(remote_data->addrs,
		                       remote_data->anum) )
		{
			log("all remote addresses are busy");
			rc = HRC_BUSY;
		}
		else
		{
			/*
			 * Fill state.node with a first valid
			 * address, try to lookup it in nodelist
			 */
			session_remote_lookup(remote_data->addrs, remote_data->anum);
			
			if( session_check_speed() )
				rc = HRC_LOW_SPEED;
		}
	}

	/*
	 * Put HELLO information to the log
	 */
	(void)yoohoo_log_sysinfo(remote_data);
	
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
		if( (remote_data->capabilities & YOOHOO_HYDRA)
		 && !(options & OPTIONS_NO_HYDRA) )
			THIS->protocol = PROT_HYDRA;
		else if( (remote_data->capabilities & YOOHOO_ZEDZAP)
		      && !(options & OPTIONS_NO_ZEDZAP) )
			THIS->protocol = PROT_ZEDZAP;
		else if( (remote_data->capabilities & YOOHOO_ZMODEM)
		      && !(options & OPTIONS_NO_ZMODEM) )
			THIS->protocol = PROT_ZMODEM;
		else /* NCP */
		{
			THIS->protocol = PROT_NOPROT;
			rc = HRC_NO_PROTOS;
		}
		
		/*
		 * Create mail/files queue
		 */
		session_create_files_queue(remote_data->addrs, remote_data->anum);
		
		/*
		 * Set FREQ processor status
		 */
		if( rc == HRC_OK && THIS->protocol != PROT_NOPROT )
			session_set_freqs_status();
		else
			state.reqstat = REQS_DISABLED;
	}
	
	/*
	 * Prepare ``local_data'' structure
	 */
	(void)yoohoo_set_sysinfo(local_data, rc, THIS->protocol);
	
	if( yoohoo_send_hello(local_data) && rc == HRC_OK )
		rc = HRC_OTHER_ERR;
	
	return rc;
}

int yoohoo_outgoing(s_handshake_protocol *THIS)
{
	s_yoohoo_sysinfo *remote_data = NULL;
	s_yoohoo_sysinfo *local_data = NULL;
	
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	ASSERT(THIS->local_data);
	
	remote_data = (s_yoohoo_sysinfo *)THIS->remote_data;
	local_data = (s_yoohoo_sysinfo *)THIS->local_data;
	
	/*
	 * Set FREQ processor status
	 */
	session_set_freqs_status();
	
	/*
	 * Prepare ``local_data'' structure
	 */
	(void)yoohoo_set_sysinfo(local_data, HRC_OK, THIS->protocol);

	if( yoohoo_send_hello(local_data) )
		return HRC_OTHER_ERR;
	
	if( yoohoo_recv_hello(remote_data) )
		return HRC_OTHER_ERR;
	
	/*
	 * Put EMSI information to the log
	 */
	(void)yoohoo_log_sysinfo(remote_data);
	
	/*
	 * Make sure expected address was presented
	 */
	if( session_addrs_check_genuine(remote_data->addrs,
	                                remote_data->anum,
	                                state.node.addr) )
		return HRC_NO_ADDRESS;
		
	/*
	 * Check password(s)
	 */
	if( session_addrs_check(remote_data->addrs, remote_data->anum,
	                        remote_data->passwd, NULL, 0) )
		return HRC_BAD_PASSWD;
	
	/*
	 * Lock (create BSY) remote addresses
	 */
	(void)session_addrs_lock(remote_data->addrs,
	                         remote_data->anum);
	
	/*
	 * Set protocol we will use ("options" ignored)
	 */
	if( remote_data->capabilities & YOOHOO_HYDRA )
		THIS->protocol = PROT_HYDRA;
	else if( remote_data->capabilities & YOOHOO_JANUS )
		THIS->protocol = PROT_JANUS;
	else if( remote_data->capabilities & YOOHOO_ZEDZAP )
		THIS->protocol = PROT_ZEDZAP;
	else if( remote_data->capabilities & YOOHOO_ZMODEM )
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
	(void)session_set_send_options();

	/*
	 * Create mail/files queue
	 */
	session_create_files_queue(remote_data->addrs,
	                           remote_data->anum);

	return HRC_OK;
}

s_faddr *yoohoo_remote_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);
	
	if( ((s_yoohoo_sysinfo *)THIS->remote_data)->anum > 0 )
		return &((s_yoohoo_sysinfo *)THIS->remote_data)->addrs[0].addr;
	
	return NULL;
}

char *yoohoo_remote_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_yoohoo_sysinfo *)THIS->remote_data)->passwd[0] )
		return ((s_yoohoo_sysinfo *)THIS->remote_data)->passwd;

	return NULL;
}

char *yoohoo_remote_sysop_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_yoohoo_sysinfo *)THIS->remote_data)->sysop[0] )
		return ((s_yoohoo_sysinfo *)THIS->remote_data)->sysop;

	return NULL;
}

char *yoohoo_remote_system_name(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->remote_data);

	if( ((s_yoohoo_sysinfo *)THIS->remote_data)->system[0] )
		return ((s_yoohoo_sysinfo *)THIS->remote_data)->system;

	return NULL;
}

s_faddr *yoohoo_local_address(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);

	if( ((s_yoohoo_sysinfo *)THIS->local_data)->anum > 0 )
		return &((s_yoohoo_sysinfo *)THIS->local_data)->addrs[0].addr;
	
	return NULL;
}

char *yoohoo_local_password(s_handshake_protocol *THIS)
{
	ASSERT(THIS);
	ASSERT(THIS->local_data);
	
	if( ((s_yoohoo_sysinfo *)THIS->local_data)->passwd[0] )
		return ((s_yoohoo_sysinfo *)THIS->local_data)->passwd;
	
	return NULL;
}

