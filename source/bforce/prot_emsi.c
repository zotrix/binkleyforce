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
 *
 *	BUG: When we resend EMSIDAT packet, it's TRX# field is most probable
 *	     out of date.
 */

#include "includes.h"
#include "confread.h"
#include "version.h"
#include "logger.h"
#include "util.h"
#include "io.h"
#include "nodelist.h"
#include "session.h"
#include "prot_common.h"
#include "prot_emsi.h"

/*
 *  #define EMSI_INQ	"**EMSI_INQC816"
 *  #define EMSI_REQ	"**EMSI_REQA77E"
 *  #define EMSI_ACK	"**EMSI_ACKA490"
 *  #define EMSI_NAK	"**EMSI_NAKEEC3"
 *  #define EMSI_CLI	"**EMSI_CLIFA8C"
 *  #define EMSI_ICI	"**EMSI_ICI2D73"
 *  #define EMSI_HBT	"**EMSI_HBTEAEE"
 *  #define EMSI_IRQ	"**EMSI_IRQ8E08"
 */

typedef struct tx_emsidat {
	int caller;
	int tries;
	time_t mast_timer;
	time_t sync_timer;
	char buf[13];
	int emsi_seq; 
	int nakcount;		/* count received EMSI_NAK */
	s_emsi *local_emsi;
} s_tx_emsidat;

typedef struct rx_emsidat {
	int caller;
	int rx_ack;		/* strip second EMSI_ACK */
	int tries;
	int tries_recv;
	time_t mast_timer;
	time_t sync_timer;
	char buf[13];
	int emsi_seq;
	s_emsi *remote_emsi;
} s_rx_emsidat;

/*
 *  State machine (TODO: REMOVE!) Now it is used only by EMSI protocol.
 */
static int state_machine(s_states *sm, void *data)
{
	signed int cur_state = 0;

	do
	{
		DEB((D_STATEM, "state_machine: entering with state \"%s\" (%d)",
			sm[cur_state].st_name, cur_state));
		cur_state = (*(sm[cur_state].proc))(data);
		DEB((D_STATEM, "state_machine: return \"%s\" (%d)",
			(cur_state>0)?sm[cur_state].st_name:"finish", cur_state));
	}
	while( cur_state > 0 );
	
	return(cur_state);
}

/* ------------------------------------------------------------------------- */
/* Transmite EMSI_DAT packet state machine routines                          */
/* ------------------------------------------------------------------------- */

/* SM0 */
static int sm_tx_init(s_tx_emsidat *d)
{
	timer_set(&d->mast_timer, 60);

	return(SM1);
}

/* SM1 */
static int sm_tx_senddat(s_tx_emsidat *d)
{
	char *emsi_dat = NULL;
	char buf[8];
	
	if( ++d->tries > 6 )
	{
		log("too many tries sending emsi data");
		return(SME);
	}
	
	if( d->tries > 1 )
		log("emsi data send - retry %d", d->tries - 1);
	
	DEB((D_HSHAKE, "sm_tx_senddat: try number %d", d->tries));
	
	if( (emsi_dat = emsi_createdat(d->local_emsi)) == NULL )
	{
		log("cannot create emsi data packet");
		return(SME);
	}
	
	DEB((D_HSHAKE, "sm_tx_senddat: emsi: \"%s\"", emsi_dat));
	
	if( tty_puts(emsi_dat, 60) < 0 )
	{
		free(emsi_dat);
		return SME;
	}
	
	/*
	 * Calculate and send CRC-16 of our emsi packet
	 */
	sprintf(buf, "%04hX\r",
		(short unsigned)getcrc16xmodem(emsi_dat+2, strlen(emsi_dat+2)));
	
	free(emsi_dat); emsi_dat = NULL;
	
	if( PUTSTR(buf) < 0 || FLUSHOUT() < 0 )
		return SME;
	
	return SM2;
}

/* SM2 */
static int sm_tx_waitseq(s_tx_emsidat *d)
{
	int rc, pos = 0;
	
	timer_set(&d->sync_timer, (d->tries > 1) ? 20 : 10);
	
	while(1)
	{
		if( timer_expired(d->mast_timer) )
		{
			DEB((D_HSHAKE, "sm_tx_waitseq: master timer expired"));
			log("master timer expired");
			return(SME);
		}
		
		if( timer_expired(d->sync_timer) )
		{
			DEB((D_HSHAKE, "sm_tx_waitseq: sync timer expired"));
			return(SM1);
		}
		
		if( (rc = GETCHAR(1)) < 0 )
		{
			if( rc != TTY_TIMEOUT )
			{
				DEB((D_HSHAKE, "sm_rx_waitseq: got ERROR/HANGUP"));
				return SME;
			}
		}
		else if( rc == XON || rc == XOFF )
		{
			/* Do nothing. Drop them down */
		}
		else if( rc == '*' )
		{
			memset(d->buf, '\0', sizeof(d->buf));
			pos = 0;
			d->emsi_seq = 1;
		}
		else if( d->emsi_seq && rc > ' ' && rc < '~' )
		{
			if( pos < sizeof(d->buf)-1 )
			{
				d->buf[pos++] = rc;
				d->buf[pos  ] = '\0';
			}
			
			if( pos == (sizeof(d->buf) - 1) )
			{
				DEB((D_HSHAKE, "sm_tx_waitseq: emsi buffer full \"%s\"", d->buf));
				
				d->emsi_seq = 0;
				
				if( !strncasecmp(d->buf, "EMSI_REQA77E", 12) )
				{
					/* Do nothing. Drop it down */
				}
				else if( !strncasecmp(d->buf, "EMSI_ACKA490", 12) )
				{
					/*
					 * Remote acknowleged our
					 * emsi data packet. exit.
					 */
					return SM0;
				}
				else if( !strncasecmp(d->buf, "EMSI_NAKEEC3", 12) )
				{
					/*
					 * Remote failed on our emsi data
					 * packet. Resend EMSI_DAT only after
					 * first EMSI_NAK
					 */
					DEB((D_HSHAKE, "sm_tx_waitseq: got NAK"));
					if( !d->nakcount++ )
						return SM1;
				}
				else if( !strncasecmp(d->buf, "EMSI_INQC816", 12) )
				{
					/*
					 * Do nothing. Just update sync timer
					 */
					return SM2;
				}
				else if( !strncasecmp(d->buf, "EMSI_DAT", 8) )
				{
					/*
					 * 1) We are calling system
					 * We got an echo of our sequence,
					 * possible they don't know about
					 * FIDO. yet :) Resend EMSI_INQ
					 * again.
					 * 2) We are asnwering system
					 * Think where is a stupid mailer
					 * that didn't seen our EMSI_ACK,
					 * so send it again in hope to the
					 * best
					 */
					DEB((D_HSHAKE, "sm_tx_waitseq: got echo \"%s\"",
							string_printable(d->buf)));

					if( d->caller )
					{
						/* Wait for a login prompt */
						sleep(2);
						
						if( PUTSTR("**EMSI_INQC816\r") < 0
						 || FLUSHOUT() < 0 )
							return SME;

						/* Wait for a password prompt */
						sleep(2);
						
						if( PUTSTR("**EMSI_INQC816\r") < 0
						 || FLUSHOUT() < 0 )
							return SME;
						
						return SM1;
					}
					else
					{
						/* Send acknowlegment */
						if( PUTSTR("**EMSI_ACKA490\r**EMSI_ACKA490\r") < 0 )
							return SME;
						if( FLUSHOUT() < 0 )
							return SME;
					}
				}
				else
				{
					DEB((D_HSHAKE, "got unexpected emsi sequence: \"%s\"",
							string_printable(d->buf)));
					log("got unexpected emsi sequence \"%s\"",
							string_printable(d->buf));
				}
			}
		}
		else if( d->emsi_seq )
		{
			d->emsi_seq = 0;
			DEB((D_HSHAKE, "sm_tx_waitseq: bad character 0x%02x in \"%s\"",
					rc, string_printable(d->buf)));
		}
	}

	return SME;
}

static s_states st_tx_emsidat[] =
{
	{ "sm_tx_init",		sm_tx_init	},
	{ "sm_tx_senddat",	sm_tx_senddat	},
	{ "sm_tx_waitseq",	sm_tx_waitseq	}
};

int emsi_send_emsidat(s_emsi *local_emsi)
{
	s_tx_emsidat d;
	
	memset(&d, '\0', sizeof(s_tx_emsidat));
	
	d.caller = state.caller;
	d.local_emsi = local_emsi;
	
	return state_machine(st_tx_emsidat, &d) ? 1 : 0;
}

/* ------------------------------------------------------------------------- */
/* Receive EMSI_DAT packet state machine routines                            */
/* ------------------------------------------------------------------------- */

/* SM0 */
static int sm_rx_init(s_rx_emsidat *d)
{
	timer_set(&d->mast_timer, 60);
	timer_set(&d->sync_timer, 20);
	
	return SM1;
}

/* SM1 */
static int sm_rx_sendnak(s_rx_emsidat *d)
{
	if( ++d->tries > 6 )
	{
		log("too many tries resyncing with remote");
		return(SME);
	}
	
	DEB((D_HSHAKE, "sm_rx_sendnak: try number %d", d->tries));
	
	if( !d->caller )
	{
		if( conf_boolean(cf_emsi_slave_sends_nak)
		 && PUTSTR("**EMSI_NAKEEC3\r") < 0 )
			return SME;
		if( PUTSTR("**EMSI_REQA77E\r") < 0 )
			return SME;
	}
	else if( d->tries > 1 )
	{
		if( PUTSTR("**EMSI_NAKEEC3\r") < 0 )
			return SME;
	}

	if( FLUSHOUT() < 0 )
		return SME;
	
	return SM2;
}

/* SM2 */
static int sm_rx_waitseq(s_rx_emsidat *d)
{
	int rc, pos = 0;
	
	timer_set(&d->sync_timer, 20);

	while(1)
	{
		if( timer_expired(d->mast_timer) )
		{
			DEB((D_HSHAKE, "sm_rx_waitseq: master timer expired"));
			log("master timer expired");
			return SME;
		}
		
		if( timer_expired(d->sync_timer) )
		{
			DEB((D_HSHAKE, "sm_rx_waitseq: sync timer expired"));
			return SM1;
		}
		
		if( (rc = GETCHAR(1)) < 0 )
		{
			if( rc != TTY_TIMEOUT )
			{
				DEB((D_HSHAKE, "sm_rx_waitseq: got ERROR/HANGUP"));
				return SME;
			}
		}
		else if( rc == XON || rc == XOFF )
		{
			/* Do nothing. Drop them down */
		}
		else if( rc == '*' )
		{
			memset(d->buf, '\0', sizeof(d->buf));
			pos = 0;
			d->emsi_seq = 1;
		}
		else if( d->emsi_seq && rc > ' ' && rc < '~' )
		{
			if( pos < sizeof(d->buf)-1 )
			{
				d->buf[pos++] = rc;
				d->buf[pos  ] = '\0';
			}
			
			if( pos == sizeof(d->buf)-1 )
			{
				DEB((D_HSHAKE, "sm_rx_waitseq: emsi buffer full \"%s\"", d->buf));
				
				d->emsi_seq = 0;
				
				if( d->caller && !strncasecmp(d->buf, "EMSI_ACKA490", 12) )
				{
					/* Do nothing. Drop it down. */
				}
				else if( !strncasecmp(d->buf, "EMSI_HBTEAEE", 12) )
				{
					/*
					 * Remote wants some time to think
					 */
					return SM2;
				}
				else if( !strncasecmp(d->buf, "EMSI_DAT", 8) )
				{
					/*
					 * Start receiving emsi data packet
					 */
					return SM3;
				}
				else if( !strncasecmp(d->buf, "EMSI_INQC816", 12) )
				{
				        return SM2;
				}
				else
				{
					DEB((D_HSHAKE, "got unexpected emsi sequence: \"%s\"",
							string_printable(d->buf)));
					log("got unexpected emsi sequence \"%s\"",
							string_printable(d->buf));
				}
			}
		}
		else if( d->emsi_seq )
		{
			d->emsi_seq = 0;
			DEB((D_HSHAKE, "sm_rx_waitseq: bad character 0x%02x in \"%s\"",
					rc, string_printable(d->buf)));
		}
	}

	return SME;
}

/* SM3 */
static int sm_rx_getdat(s_rx_emsidat *d)
{
	int rc = 0;
	int pos = 0;
	int emsi_len = 0;
	short unsigned ourcrc;
	short unsigned remcrc;
	char *emsi_dat = NULL;
	
	if( d->tries_recv++ )
		log("emsi data receive - retry %d", d->tries_recv);
	
	/*
	 * At this point, there is a EMSI_DATXXXX packet in
	 * the buffer, there XXXX is the hex length of emsi
	 * data packet
	 */
	if( strspn(d->buf+8, "0123456789abcdefABCDEF") != 4 )
		return SM1;
	
	if( sscanf(d->buf+8, "%04x", &emsi_len) != 1 )
		return SM1;
	
	/*
	 * Don't receive emsi packet's longer our "limit"
	 */
	if( emsi_len > EMSI_MAXDAT )
	{
		CLEARIN();
		log("emsi data packet too long %db (maximum %db allowed)",
				emsi_len, EMSI_MAXDAT);
		return SM1;
	}
	
	/* Increase packet's length on CRC length */
	emsi_len += 16;
	
	emsi_dat = (char*)xmalloc(emsi_len + 1);
	strncpy(emsi_dat, d->buf, 12);
	pos = 12;
	
	while( pos < emsi_len )
	{
		if( timer_expired(d->mast_timer) )
			break;
		
		if( (rc = GETCHAR(1)) < 0 )
		{
			if( rc != TTY_TIMEOUT )
				break;
		}
		else
		{
			emsi_dat[pos++] = (unsigned char)rc;
			emsi_dat[pos  ] = '\0';
		}
	}
	
	if( pos != emsi_len )
	{
		/* Fatal error occured */
		DEB((D_HSHAKE, "sm_rx_getdat: can't get emsi_dat packet"));
		DEB((D_HSHAKE, "sm_rx_getdat: buffer: \"%s\"",
				string_printable(emsi_dat)));
		free(emsi_dat);
		return SME;
	}

	DEB((D_HSHAKE, "sm_rx_getdat: got \"%s\"",
			string_printable(emsi_dat)));

	/*
	 * Get CRC given by remote
	 */
	if( sscanf(emsi_dat + emsi_len - 4 , "%04hX", &remcrc) != 1 )
	{
		log("bad emsi data packet (can't get CRC16)");
		free(emsi_dat);
		
		return(SM1);
	}
	
	/*
	 * Calculate our own crc of the data packet
	 */
	ourcrc = getcrc16xmodem(emsi_dat, emsi_len-4);
	
	/*
	 * Compare our and remote CRCs
	 */
	if( ourcrc != remcrc )
	{
		DEB((D_HSHAKE, "sm_rx_getdat: our = %hX, remote = %hX",	ourcrc, remcrc));
		log("got emsi data packet with bad CRC");
		free(emsi_dat);
		
		return SM1;
	}
	
	/*
	 * Parse received emsi data packet. All obtained
	 * information will be stored in d->remote_emsi
	 */
	rc = emsi_parsedat(emsi_dat+12, d->remote_emsi);
	
	free(emsi_dat);
		
	if( rc )
	{
		log("received invalid emsi data packet");
		return SM1;
	}
	
	/* Send acknowlegment */
	if( PUTSTR("**EMSI_ACKA490\r**EMSI_ACKA490\r") < 0 )
		return SME;
	if( FLUSHOUT() < 0 )
		return SME;
	
	return SM0;
}

static s_states st_rx_emsidat[] =
{
	{ "sm_rx_init",		sm_rx_init	},
	{ "sm_rx_sendnak",	sm_rx_sendnak	},
	{ "sm_rx_waitseq",	sm_rx_waitseq   },
	{ "sm_rx_getdat",	sm_rx_getdat    }
};

int emsi_recv_emsidat(s_emsi *remote_emsi)
{
	s_rx_emsidat d;
	
	memset(&d, '\0', sizeof(s_rx_emsidat));
	
	d.caller = state.caller;
	d.remote_emsi = remote_emsi;
	
	return state_machine(st_rx_emsidat, &d) ? 1 : 0;
}

/* ------------------------------------------------------------------------- */
/* Fill/update our outgoing emsi oriented structure                          */
/* ------------------------------------------------------------------------- */
void emsi_set_sysinfo(s_emsi *emsi, s_emsi *remote_emsi, int hrc,
                      e_protocol protocol)
{
	char buf[64];
	s_cval_entry *addr_ptr;
	s_cval_entry *hide_ptr;
	s_faddr *primary = NULL;
	
	const long options      = conf_options(cf_options);
	const long speed        = conf_number(cf_max_speed);
	const char *p_systname  = conf_string(cf_system_name);
	const char *p_location  = conf_string(cf_location);
	const char *p_sysopname = conf_string(cf_sysop_name);
	const char *p_phone     = conf_string(cf_phone);
	const char *p_flags     = conf_string(cf_flags);
	const char *p_emsioh    = conf_string(cf_emsi_OH_time);
	const char *p_emsifr    = conf_string(cf_emsi_FR_time);

	/* free previously allocated memory */
	if( emsi->addrs ) free(emsi->addrs);
		
	memset(emsi, '\0', sizeof(s_emsi));
	
	/* ----------------------------------------------------------------- */
	/* Data for {EMSI} field                                             */
	/* ----------------------------------------------------------------- */
	emsi->have_emsi = 1;
	
	/* Set best primary address */
	primary = session_get_bestaka(state.node.addr);
		
	/* Add primary address */
	if( primary )
		session_addrs_add(&emsi->addrs, &emsi->anum, *primary);

	/* Add other AKAs */
	for( addr_ptr = conf_first(cf_address); addr_ptr;
	     addr_ptr = conf_next(addr_ptr) )
	{
		for( hide_ptr = conf_first(cf_hide_our_aka); hide_ptr;
		     hide_ptr = conf_next(hide_ptr) )
		{
			if( !ftn_addrcomp(hide_ptr->d.falist.addr, addr_ptr->d.falist.addr) )
				break;
		}
		
		if( !hide_ptr && primary != &addr_ptr->d.falist.addr )
			session_addrs_add(&emsi->addrs, &emsi->anum,
					addr_ptr->d.falist.addr);
	}
	
	if( emsi->anum == 0 )
		log("warning: no addresses will be presented to remote");

	/* session password */
	if( state.caller )
	{
		session_get_password(state.node.addr, emsi->passwd, sizeof(emsi->passwd));
	}
	else if( hrc == HRC_OK )
	{
		/* Satisfy remote with their password */
		const char *p = state.handshake->remote_password(state.handshake);
		if( p )
			strnxcpy(emsi->passwd, p, sizeof(emsi->passwd));
	}
	
	/* link codes */
	if( state.caller ) /* CALLER */
	{
		if( (options & OPTIONS_NO_RH1)   != OPTIONS_NO_RH1
		 && (options & OPTIONS_NO_HYDRA) != OPTIONS_NO_HYDRA)
			{ emsi->linkcodes.RH1 = 1; }
		
		if( (options & OPTIONS_NO_PICKUP) == OPTIONS_NO_PICKUP )
			{ emsi->linkcodes.NPU = 1; }
		else
			{ emsi->linkcodes.PUA = 1; }
			
		if( (options & OPTIONS_MAILONLY) == OPTIONS_MAILONLY )
			{ emsi->linkcodes.HXT = 1; }
		
		if( (options & OPTIONS_NO_EMSI_II) != OPTIONS_NO_EMSI_II )
		{
			/* EMSI-II */
			if( (options & OPTIONS_HOLDREQ) != OPTIONS_HOLDREQ )
				{ emsi->linkcodes.RMA = 1; }
		}
		else
		{
			/* EMSI-I */
			emsi->linkcodes.N81 = 1;
		}
	}
	else if( (remote_emsi->compcodes.EII == 1)
	      && (options & OPTIONS_NO_EMSI_II) != OPTIONS_NO_EMSI_II )
	{
		/* ANSWER/EMSI-II */
		if( remote_emsi->linkcodes.RH1 && protocol == PROT_HYDRA )
			{ emsi->linkcodes.RH1 = 1; }
		
		if( state.reqstat == REQS_ALLOW && remote_emsi->linkcodes.RMA )
			{ emsi->linkcodes.RMA = 1; }
		else if( state.reqstat == REQS_NOTALLOW )
			{ emsi->linkcodes.HRQ = 1; }
		
		if( (options & OPTIONS_MAILONLY) == OPTIONS_MAILONLY )
			{ emsi->linkcodes.HXT = 1; }
	}
	else
	{
		/* ANSWER/EMSI-I */
		if( remote_emsi->linkcodes.RH1 && protocol == PROT_HYDRA )
			{ emsi->linkcodes.RH1 = 1; }
		
		if( state.reqstat == REQS_NOTALLOW )
			{ emsi->linkcodes.HRQ = 1; }
		
		if( (options & OPTIONS_MAILONLY) == OPTIONS_MAILONLY )
			{ emsi->linkcodes.HXT = 1; }
	}
	
	/* compatibility codes */
	if( state.caller )
	{
		if( (options & OPTIONS_NO_EMSI_II) != OPTIONS_NO_EMSI_II )
		{
			/* EMSI-II */
			emsi->compcodes.EII = 1;
		}
		else
		{
			/* EMSI-I */
			emsi->compcodes.ARC = 1;
			emsi->compcodes.XMA = 1;
		}
		if( (options & OPTIONS_NO_ZMODEM) != OPTIONS_NO_ZMODEM )
			{ emsi->compcodes.ZMO = 1; }
		if( (options & OPTIONS_NO_ZEDZAP) != OPTIONS_NO_ZEDZAP )
			{ emsi->compcodes.ZAP = 1; }
		if( (options & OPTIONS_NO_DIRZAP) != OPTIONS_NO_DIRZAP )
			{ emsi->compcodes.DZA = 1; }
		if( (options & OPTIONS_NO_JANUS) != OPTIONS_NO_JANUS )
			{ emsi->compcodes.JAN = 1; }
		if( (options & OPTIONS_NO_HYDRA) != OPTIONS_NO_HYDRA )
			{ emsi->compcodes.HYD = 1; }
	}
	else
	{
		if( (remote_emsi->compcodes.EII == 1)
		 && (options & OPTIONS_NO_EMSI_II) != OPTIONS_NO_EMSI_II )
		{
			/* EMSI-II */
			emsi->compcodes.EII = 1;
		}
		else
		{
			/* EMSI-I */
			emsi->compcodes.ARC = 1;
			emsi->compcodes.XMA = 1;
		}
		
		if( state.reqstat == REQS_DISABLED )
			{ emsi->compcodes.NRQ = 1; }
			
		switch(protocol) {
		case PROT_NOPROT: emsi->compcodes.NCP = 1; break;
		case PROT_ZMODEM: emsi->compcodes.ZMO = 1; break;
		case PROT_ZEDZAP: emsi->compcodes.ZAP = 1; break;
		case PROT_DIRZAP: emsi->compcodes.DZA = 1; break;
		case PROT_JANUS:  emsi->compcodes.JAN = 1; break;
		case PROT_HYDRA:  emsi->compcodes.HYD = 1; break;
		default:          ASSERT(FALSE);           break;
		}
	}
	
	strnxcpy(emsi->m_pid,  BF_EMSI_NUM,  sizeof(emsi->m_pid));
	strnxcpy(emsi->m_name, BF_EMSI_NAME, sizeof(emsi->m_name));
	
	if( hrc != HRC_BAD_PASSWD )
	{
		strnxcpy(emsi->m_ver,  BF_EMSI_VER,  sizeof(emsi->m_ver));
		strnxcpy(emsi->m_reg,  BF_EMSI_REG,  sizeof(emsi->m_reg));
	}
	else
	{
		strnxcpy(emsi->m_ver, "?", sizeof(emsi->m_ver));
		strnxcpy(emsi->m_reg, "?", sizeof(emsi->m_reg));
	}

	/* ----------------------------------------------------------------- */
	/* Data for {IDENT} field                                            */
	/* ----------------------------------------------------------------- */
	emsi->have_ident = 1;
	emsi->speed = (speed > 0) ? speed : 300;
	
	strnxcpy(emsi->sysop, p_sysopname ? p_sysopname : "Unknown", sizeof(emsi->sysop));
	strnxcpy(emsi->phone, p_phone     ? p_phone     : "Unknown", sizeof(emsi->phone));
	
	switch(hrc) {
	case HRC_BAD_PASSWD:
		strnxcpy(emsi->sname,    "Bad password", sizeof(emsi->sname));
		strnxcpy(emsi->location, "Check security table", sizeof(emsi->location));
		strnxcpy(emsi->flags,    "Password Error", sizeof(emsi->flags));
		break;
	case HRC_LOW_SPEED:
		strnxcpy(emsi->sname,    "Connect speed too low", sizeof(emsi->sname));
		strnxcpy(emsi->location, "Buy new modem and call again", sizeof(emsi->location));
		sprintf(buf, "Minimal speed: %ldbps", (long)state.minspeed);
		strnxcpy(emsi->flags,    buf, sizeof(emsi->flags));
		break;
	case HRC_BUSY:
		strnxcpy(emsi->sname,    "All AKAs are busy", sizeof(emsi->sname));
		strnxcpy(emsi->location, "Possible another session is running", sizeof(emsi->location));
		strnxcpy(emsi->flags,    "Please, call later", sizeof(emsi->flags));
		break;
	default:
		strnxcpy(emsi->sname,    p_systname  ? p_systname  : "Unknown", sizeof(emsi->sname));
		strnxcpy(emsi->location, p_location  ? p_location  : "Unknown", sizeof(emsi->location));
		strnxcpy(emsi->flags,    p_flags     ? p_flags     : "MO",      sizeof(emsi->flags));
	}
	
	/* ----------------------------------------------------------------- */
	/* Data for {OHFR} field                                             */
	/* ----------------------------------------------------------------- */
	if( p_emsioh && *p_emsioh && hrc != HRC_BAD_PASSWD )
	{
		emsi->have_ohfr = 1;
		strnxcpy(emsi->oh_time, p_emsioh, sizeof(emsi->oh_time));
		if( p_emsifr && *p_emsifr )
			strnxcpy(emsi->fr_time, p_emsifr, sizeof(emsi->fr_time));
	}
	
	/* ----------------------------------------------------------------- */
	/* Data for {TRX#} field                                             */
	/* ----------------------------------------------------------------- */
	emsi->have_trx = 1;
	emsi->time = localtogmt(time(NULL));
	
	/* ----------------------------------------------------------------- */
	/* Data for {TRAF} and {MOH#} fields                                 */
	/* ----------------------------------------------------------------- */
	if( state.caller == 0 && hrc != HRC_BAD_PASSWD )
	{
		emsi->have_traf = 1;
		emsi->netmail_size = state.traff_send.netmail_size;
		emsi->arcmail_size = state.traff_send.arcmail_size;
		if ( state.traff_send.files_size )
		{
			emsi->have_moh = 1;
			emsi->files_size = state.traff_send.files_size;
		}	
	}
}

