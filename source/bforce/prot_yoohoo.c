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
#include "session.h"
#include "prot_yoohoo.h"

typedef enum {
	YS_Init,
	YS_SendHello,
	YS_WaitResp,
	YS_Done,
	YS_Error
} YooHoo_SendState;

typedef enum {
	YR_Init,
	YR_SendENQ,
	YR_WaitHeader,
	YR_TossJunk,
	YR_RecvHello,
	YR_SendNAK,
	YR_SendACK,
	YR_Done,
	YR_Error
} YooHoo_RecvState;

static char *yoohoo_putword(char *buf, int val)
{
	buf[0] = ( ((unsigned int) val)      ) & 0xff;
	buf[1] = ( ((unsigned int) val) >> 8 ) & 0xff;
	
	return buf + 2;
}

static int yoohoo_getword(const char *buf)
{
	return ( (unsigned int) ((unsigned char) buf[0])      )
	     | ( (unsigned int) ((unsigned char) buf[1]) << 8 );
}

/*****************************************************************************
 * Make ``hello'' packet from the yoohoo sysinfo structure and put
 * it to the buffer
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer (must be at least
 * 	          128 bytes)
 * 	hello     structure with the YooHoo system information
 *
 * Return value:
 * 	None
 */
static void yoohoo_put_hello(char *buffer, s_yoohoo_sysinfo *hello)
{
	char *p, *q;

	ASSERT(buffer && hello);
	
	memset(buffer, '\0', YOOHOO_HELLOLEN);
	
	p = buffer;
	p = yoohoo_putword(p, 0x6f);
	p = yoohoo_putword(p, 0x01);                 /* Hello version */
	p = yoohoo_putword(p, hello->product_code);  /* Product code */
	p = yoohoo_putword(p, hello->version_maj);   /* Major version */
	p = yoohoo_putword(p, hello->version_min);   /* Minor version */
	
	strnxcpy(p, hello->system, 60);              /* Node name */
	
	/*
	 * Add domain after the end of 'Node name'
	 * TODO: check it for buffer overflows %-I
	 */
	if( hello->anum > 0 && hello->addrs[0].addr.domain
	                    && *hello->addrs[0].addr.domain )
	{
		char *q;
		if( strlen(hello->system) + strlen(hello->addrs[0].addr.domain) > 57 )
		{
			if( strlen(hello->addrs[0].addr.domain) < 60 )
				q = p + (60 - strlen(hello->addrs[0].addr.domain));
			else
				q = p;
			
			*q++ = '\0';
		}
		else
		{
			q = p + strlen(hello->system) + 1;
		}
		strnxcpy(q, hello->addrs[0].addr.domain, 60-(q-p));
	}
	p += 60;

	strnxcpy(p, hello->sysop, 20);               /* SysOp name */
	p += 20;
	
	if( hello->anum > 0 )
	{
		p = yoohoo_putword(p, hello->addrs[0].addr.zone);
		p = yoohoo_putword(p, hello->addrs[0].addr.net);
		p = yoohoo_putword(p, hello->addrs[0].addr.node);
		p = yoohoo_putword(p, hello->addrs[0].addr.point);
	}
	else
		p += 8;
	
	strncpy(p, hello->passwd, 8);                /* Session password */
	p += 8;
	p += 8;                                      /* Reserved 8 bytes */
	p = yoohoo_putword(p, hello->capabilities);  /* Capabilities */
	p += 12;                                     /* Reserved 12 bytes */

#ifdef DEBUG
	DEB((D_HSHAKE, "yoohoo_put_hello: HELLO dump: \"%s\"",
			q = string_printable_buffer(buffer, 128)));
	if( q ) free(q);
#endif
	
	ASSERT((p - buffer) == YOOHOO_HELLOLEN);
}

/*****************************************************************************
 * Extract session information from the ``hello'' packet
 *
 * Arguments:
 * 	hello     pointer to the destination structure with the session
 * 	          information
 * 	buffer    source buffer with ``hello'' packet
 *
 * Return value:
 * 	None
 */
static int yoohoo_get_hello(s_yoohoo_sysinfo *hello, const char *buffer)
{
	s_faddr addr;
	
	ASSERT(buffer && hello);

#ifdef DEBUG
	{
		char *q = string_printable_buffer(buffer, YOOHOO_HELLOLEN);
		DEB((D_HSHAKE, "yoohoo_get_hello: HELLO dump: \"%s\"", q));
		if( q ) free(q);
	}
#endif
	
	memset(hello, '\0', sizeof(s_yoohoo_sysinfo));
	
	if( yoohoo_getword(buffer+2) != 0x01 )
		log("YooHoo hello version is %d!", yoohoo_getword(buffer+2));
	
	hello->product_code  = yoohoo_getword(buffer+4);
	hello->version_maj   = yoohoo_getword(buffer+6);
	hello->version_min   = yoohoo_getword(buffer+8);
	strnxcpy(hello->system, buffer+10, MIN(sizeof(hello->system),60+1));
	strnxcpy(hello->sysop, buffer+70, MIN(sizeof(hello->sysop),20+1));

	/*
	 * Extract address
	 */
	memset(&addr, '\0', sizeof(s_faddr));
	addr.zone  = yoohoo_getword(buffer+90);
	addr.net   = yoohoo_getword(buffer+92);
	addr.node  = yoohoo_getword(buffer+94);
	addr.point = yoohoo_getword(buffer+96);
	session_addrs_add(&hello->addrs, &hello->anum, addr);
	
	strnxcpy(hello->passwd, buffer+98, MIN(sizeof(hello->passwd),8+1));
	/* Reserved 8 bytes */
	hello->capabilities = yoohoo_getword(buffer+114);
	/* Reserved 12 bytes */

	return 0;
}

int yoohoo_send_hello(s_yoohoo_sysinfo *local_data)
{
	char hello_buffer[YOOHOO_HELLOLEN];
	unsigned short hello_crc = 0;
	YooHoo_SendState state = YS_Init;
	time_t wait_timer;
	int tries = 0;
	int rc;
	
	ASSERT(local_data);
	
	while(1)
	{
		switch(state) {
		case YS_Init:
			tries = 0;
			yoohoo_put_hello(hello_buffer, local_data);
			hello_crc = getcrc16xmodem(hello_buffer, YOOHOO_HELLOLEN);
			state = YS_SendHello;
			break;
		
		case YS_SendHello:
			if( ++tries > 10 )
			{
				log("too many tries sending hello");
				state = YS_Error;
				break;
			}
			else if( tries > 1 )
				log("yoohoo hello send - retry %d", tries);
		
			if( PUTCHAR(0x1f) < 0
			 || WRITE_TIMEOUT(hello_buffer, sizeof(hello_buffer)) < 0
			 || PUTCHAR(((unsigned int) hello_crc >> 8) & 0xff) < 0
			 || PUTCHAR(((unsigned int) hello_crc     ) & 0xff) < 0 )
			{
				state = YS_Error;
				break;
			}

			timer_set(&wait_timer, 40);
			state = YS_WaitResp;
			break;
		
		case YS_WaitResp:
			if( timer_expired(wait_timer) )
			{
				log("time out waiting for response");
				state = YS_Error;
				break;
			}

			if( (rc = GETCHAR(1)) < 0 )
			{
				if( rc != TTY_TIMEOUT )
				{
					state = YS_Error;
					break;
				}
			}
			else if( rc == XON || rc == XOFF )
			{
				/* Do nothing. Drop them down */
			}
			else if( rc == '?' || rc == ENQ )
			{
				if( rc == '?' )
					log("remote failed to receive our HELLO packet");
				state = YS_SendHello;
			}
			else if( rc == ACK )
			{
				state = YS_Done;
			}
			break;
		
		case YS_Done:
			return 0;

		case YS_Error:
			return -1;
		}	
	}
	
	return -1; /* UNREACHABLE */
}

int yoohoo_recv_hello(s_yoohoo_sysinfo *remote_data)
{
	char hello_buffer[YOOHOO_HELLOLEN];
	int crc_local = 0;
	int crc_remote = 0;
	YooHoo_RecvState state = YR_Init;
	time_t wait_timer;
	time_t junk_timer;
	int hello_pos;
	int tries = 0;
	int rc;

	ASSERT(remote_data);
	
	while(1)
	{
		switch(state) {
		case YR_Init:
			tries = 0;
			memset(hello_buffer, '\0', sizeof(hello_buffer));
			state = YR_SendENQ;
			break;
		
		case YR_SendENQ:
			if( ++tries > 3 )
			{
				log("too many tries receiving hello");
				state = YR_Error;
				break;
			}
			else if( tries > 1 )
				log("yoohoo hello recv - retry %d", tries);
		
			if( PUTCHAR(ENQ) < 0 )
			{
				state = YR_Error;
				break;
			}
			
			timer_set(&wait_timer, 120);
			state = YR_WaitHeader;
			break;

		case YR_WaitHeader:
			if( timer_expired(wait_timer) )
			{
				log("time out waiting for response");
				state = YR_Error;
				break;
			}

			if( (rc = GETCHAR(1)) < 0 )
			{
				if( rc != TTY_TIMEOUT )
				{
					state = YR_Error;
					break;
				}
			}
			else if( rc == XON || rc == XOFF )
			{
				/* Do nothing. Drop them down */
			}
			else if( rc == 0x1f )
			{
				state = YR_RecvHello;
			}
			else
			{
				timer_set(&junk_timer, 10);
				state = YR_TossJunk;
			}
			break;
			
		case YR_TossJunk:
			if( timer_expired(junk_timer) )
			{
				state = YR_SendENQ;
				break;
			}

			if( (rc = GETCHAR(1)) < 0 )
			{
				if( rc != TTY_TIMEOUT )
				{
					state = YR_Error;
					break;
				}
			}
			else if( rc == XON || rc == XOFF )
			{
				/* Do nothing. Drop them down */
			}
			else if( rc == 0x1f )
			{
				state = YR_RecvHello;
			}
			break;
		
		case YR_RecvHello:
			hello_pos = 0;
			while( (rc = GETCHAR(30)) >= 0 )
			{
				if( hello_pos < 128 )
				{
					hello_buffer[hello_pos++] = rc;
				}
				else if( hello_pos == 128 )
				{
					crc_remote = ((unsigned int) ((unsigned char) rc) << 8);
					++hello_pos;
				}
				else if( hello_pos == 129 )
				{
					crc_remote |= (unsigned int) ((unsigned char) rc);
					break;
				}
				else
					break;
			}
			
			if( hello_pos != 129 )
			{
				state = YR_Error;
				break;
			}
			
			/*
			 * Check CRC-16
			 */
			crc_local = getcrc16xmodem(hello_buffer, sizeof(hello_buffer));
			if( crc_local != crc_remote )
			{
				log("got hello packet with incorrect checksum");
				state = YR_SendNAK;
				break;
			}
			
			if( yoohoo_get_hello(remote_data, hello_buffer) )
			{
				log("got invalid hello packet");
				state = YR_SendNAK;
			}
			else
				state = YR_SendACK;
			break;

		case YR_SendNAK:
			if( PUTCHAR('?') < 0 )
				state = YR_Error;
			else
				state = YR_WaitHeader;
			break;
		
		case YR_SendACK:
			if( PUTCHAR(ACK) < 0 )
				state = YR_Error;
			else
				state = YR_Done;
			break;
		
		case YR_Done:
			return 0;

		case YR_Error:
			return -1;
		}
	}
	
	return -1; /* UNREACHABLE */
}

void yoohoo_set_sysinfo(s_yoohoo_sysinfo *local_data, int hrc,
                        e_protocol protocol)
{
	s_cval_entry *addr_ptr;
	s_faddr *primary = NULL;
	
	const long options   = conf_options(cf_options);
	const char *p_system = conf_string(cf_system_name);
	const char *p_sysop  = conf_string(cf_sysop_name);

	memset(local_data, '\0', sizeof(s_yoohoo_sysinfo));
	
	/* Set best primary address */
	primary = session_get_bestaka(state.node.addr);
	
	/*
	 * Set our local address
	 */
	if( primary )
		session_addrs_add(&local_data->addrs, &local_data->anum, *primary);
	else if( (addr_ptr = conf_first(cf_address)) )
		session_addrs_add(&local_data->addrs, &local_data->anum, addr_ptr->d.falist.addr);

	if( !local_data->anum )
		log("warning: no addresses will be presented to remote");

	/*
	 * Set session password
	 */
	if( state.caller )
	{
		session_get_password(state.node.addr,
		                     local_data->passwd, sizeof(local_data->passwd));
	}
	else if( hrc == HRC_OK )
	{
		/* Satisfy remote with their password */
		const char *p = state.handshake->remote_password(state.handshake);
		if( p )
			strnxcpy(local_data->passwd, p, sizeof(local_data->passwd));
	}
	
	if( !state.caller )
	{
		if( state.reqstat != REQS_ALLOW )
			local_data->capabilities |= YOOHOO_WZ_FREQ;
	}
	
	/* compatibility codes */
	if( state.caller )
	{
		if( (options & OPTIONS_NO_ZMODEM) != OPTIONS_NO_ZMODEM )
			local_data->capabilities |= YOOHOO_ZMODEM;
		if( (options & OPTIONS_NO_ZEDZAP) != OPTIONS_NO_ZEDZAP )
			local_data->capabilities |= YOOHOO_ZEDZAP;
		if( (options & OPTIONS_NO_JANUS) != OPTIONS_NO_JANUS )
			local_data->capabilities |= YOOHOO_JANUS;
		if( (options & OPTIONS_NO_HYDRA) != OPTIONS_NO_HYDRA )
			local_data->capabilities |= YOOHOO_HYDRA;
	}
	else
	{
		switch(protocol) {
		case PROT_ZMODEM: local_data->capabilities |= YOOHOO_ZMODEM; break;
		case PROT_ZEDZAP: local_data->capabilities |= YOOHOO_ZEDZAP; break;
		case PROT_JANUS:  local_data->capabilities |= YOOHOO_JANUS;  break;
		case PROT_HYDRA:  local_data->capabilities |= YOOHOO_HYDRA;  break;
		default:          break;
		}
	}
	
	local_data->product_code = BF_PRODCODE;

	if( hrc == HRC_BAD_PASSWD )
	{
		local_data->version_maj = 99;
		local_data->version_min = 99; /* TODO */
	}
	else
	{
		local_data->version_maj = 0;
		local_data->version_min = 1;
	}
	
	strnxcpy(local_data->sysop, p_sysop ? p_sysop : "Unknown", sizeof(local_data->sysop));
	
	switch(hrc) {
	case HRC_BAD_PASSWD:
		strnxcpy(local_data->system, "Bad password", sizeof(local_data->system));
		break;
	case HRC_LOW_SPEED:
		strnxcpy(local_data->system, "Connect speed too low", sizeof(local_data->system));
		break;
	case HRC_BUSY:
		strnxcpy(local_data->system, "All AKAs are busy", sizeof(local_data->system));
		break;
	default:
		strnxcpy(local_data->system, p_system ? p_system : "Unknown", sizeof(local_data->system));
	}
}

/*****************************************************************************
 * Write system information to the log
 *
 * Arguments:
 * 	yoohoo    structure with the system information
 *
 * Return value:
 * 	None
 */
void yoohoo_log_sysinfo(s_yoohoo_sysinfo *yoohoo)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	
	if( yoohoo->anum )
		for( i = 0; i < yoohoo->anum; i++ )
		{
			log("   Address : %s", ftn_addrstr(abuf, yoohoo->addrs[i].addr));
		}
	else
		log("   Address : <none>");
	
	if( yoohoo->system[0] )
		log("    System : %s", string_printable(yoohoo->system));

#ifdef BFORCE_LOG_PASSWD
	if( yoohoo->passwd[0] )
		log("  Password : %s", string_printable(yoohoo->passwd));
#endif

	if( yoohoo->sysop[0] )
		log("     SysOp : %s", string_printable(yoohoo->sysop));

	if( yoohoo->product_code || yoohoo->version_maj || yoohoo->version_min )
	{
		log("    Mailer : %s [%02x] %d.%d",
			"?", yoohoo->product_code,
			yoohoo->version_maj, yoohoo->version_min);
	}
}

