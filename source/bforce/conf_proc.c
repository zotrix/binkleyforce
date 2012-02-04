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
#include "outbound.h" /* Required only for ``proc_filebox()'' */

struct tab_options {
const	char *keystr;
	unsigned long value_no;
	unsigned long value_yes;
} options[] = {
	{ "Zmodem",   OPTIONS_NO_ZMODEM,   0                },
	{ "ZedZap",   OPTIONS_NO_ZEDZAP,   0                },
	{ "DirZap",   OPTIONS_NO_DIRZAP,   0                },
	{ "Janus",    OPTIONS_NO_JANUS,    0                },
	{ "Hydra",    OPTIONS_NO_HYDRA,    0                },
	{ "BinkP",    OPTIONS_NO_BINKP,    0                },
	{ "TCP",      OPTIONS_NO_TCP,      0                },
	{ "Chat",     OPTIONS_NO_CHAT,     0                },
	{ "FTS1",     OPTIONS_NO_FTS1,     0                },
	{ "YooHoo",   OPTIONS_NO_YOOHOO,   0                },
	{ "EMSI",     OPTIONS_NO_EMSI,     0                },
	{ "EMSI-II",  OPTIONS_NO_EMSI_II,  0                },
	{ "Freqs",    OPTIONS_NO_FREQS,    0                },
	{ "MailOnly", 0,                   OPTIONS_MAILONLY },
	{ "HoldXT"  , 0,                   OPTIONS_HOLDXT   },
	{ "HoldReq",  0,                   OPTIONS_HOLDREQ  },
	{ "HoldAll",  0,                   OPTIONS_HOLDALL  },
	{ "HoldHold", 0,                   OPTIONS_HOLDHOLD },
	{ "PickUp",   OPTIONS_NO_PICKUP,   0                },
	{ "RH1",      OPTIONS_NO_RH1,      0                },
	{ "Intro",    OPTIONS_NO_INTRO,    0                },
	{ NULL,       0,                   0                }
};

struct tab_resptypes {
const	char *str;
	int value;
	int retv;
} resptypes[] = {
	{ "Connect",     RESPTYPE_CONNECT,     0  },
	{ "Busy",        RESPTYPE_BUSY,        11 },
	{ "Nocarrier",   RESPTYPE_NOCARRIER,   12 },
	{ "Nodialtone",  RESPTYPE_NODIALTONE,  13 },
	{ "Noanswer",    RESPTYPE_NOANSWER,    14 },
	{ "Error",       RESPTYPE_ERROR,       15 },
	{ NULL,          0,                    0  }
};

#define CONF_KEY(name, type)    { #name, type,  NULL, cf_##name }
#define CONF_END()              { NULL,  0,     NULL, 0         }

s_conf_entry bforce_config[BFORCE_NUMBER_OF_KEYWORDS+1] = {
	CONF_KEY(address,                    CT_ADDRESS),
	CONF_KEY(amiga_outbound_directory,   CT_PATH),
	CONF_KEY(binkp_timeout,              CT_NUMBER),
	CONF_KEY(daemon_circle,              CT_NUMBER),
	CONF_KEY(daemon_circle_crash,        CT_NUMBER),
	CONF_KEY(daemon_circle_direct,       CT_NUMBER),
	CONF_KEY(daemon_circle_immed,        CT_NUMBER),
	CONF_KEY(daemon_circle_modem,        CT_NUMBER),
	CONF_KEY(daemon_circle_normal,       CT_NUMBER),
	CONF_KEY(daemon_circle_rescan,       CT_NUMBER),
	CONF_KEY(daemon_maxclients_modem,    CT_NUMBER),
	CONF_KEY(daemon_maxclients_tcpip,    CT_NUMBER),
	CONF_KEY(daemon_pid_file,            CT_STRING),
	CONF_KEY(delay_files_recv,           CT_STRING),
	CONF_KEY(delay_files_send,           CT_STRING),
	CONF_KEY(disable_aka_matching,       CT_BOOLEAN),
	CONF_KEY(domain,                     CT_DOMAIN),
	CONF_KEY(emsi_FR_time,               CT_STRING),
	CONF_KEY(emsi_OH_time,               CT_STRING),
	CONF_KEY(emsi_slave_sends_nak,       CT_BOOLEAN),
	CONF_KEY(filebox,                    CT_FILEBOX),
	CONF_KEY(filebox_directory,          CT_PATH),
	CONF_KEY(flags,                      CT_STRING),
	CONF_KEY(flo_translate,              CT_TRANSLATE),
	CONF_KEY(freq_alias_list,            CT_STRING),
	CONF_KEY(freq_dir_list,              CT_STRING),
	CONF_KEY(freq_ignore_masks,          CT_STRING),
	CONF_KEY(freq_limit_number,          CT_NUMBER),
	CONF_KEY(freq_limit_size,            CT_NUMBER),
	CONF_KEY(freq_limit_time,            CT_NUMBER),
	CONF_KEY(freq_min_speed,             CT_NUMBER),
	CONF_KEY(freq_srif_command,          CT_STRING),
	CONF_KEY(hide_our_aka,               CT_ADDRESS),
	CONF_KEY(history_file,               CT_STRING),
	CONF_KEY(hydra_options,              CT_OPTIONS),
	CONF_KEY(hydra_mincps_recv,          CT_CONNLIST),
	CONF_KEY(hydra_mincps_send,          CT_CONNLIST),
	CONF_KEY(hydra_tx_window,            CT_NUMBER),
	CONF_KEY(hydra_rx_window,            CT_NUMBER),
	CONF_KEY(inbound_directory,          CT_PATH),
	CONF_KEY(location,                   CT_STRING),
	CONF_KEY(log_file,                   CT_STRING),
	CONF_KEY(log_file_daemon,            CT_STRING),
	CONF_KEY(max_speed,                  CT_NUMBER),
	CONF_KEY(maxtries,                   CT_TRIES),
	CONF_KEY(maxtries_nodial,            CT_TRIES),
	CONF_KEY(maxtries_noansw,            CT_TRIES),
	CONF_KEY(maxtries_noconn,            CT_TRIES),
	CONF_KEY(maxtries_hshake,            CT_TRIES),
	CONF_KEY(maxtries_sessions,          CT_TRIES),
	CONF_KEY(min_cps_recv,               CT_NUMBER),
	CONF_KEY(min_cps_send,               CT_NUMBER),
	CONF_KEY(min_cps_time,               CT_NUMBER),
	CONF_KEY(min_free_space,             CT_NUMBER),
	CONF_KEY(min_speed_in,               CT_NUMBER),
	CONF_KEY(min_speed_out,              CT_NUMBER),
	CONF_KEY(mode_netmail,               CT_FILEMODE),
	CONF_KEY(mode_arcmail,               CT_FILEMODE),
	CONF_KEY(mode_request,               CT_FILEMODE),
	CONF_KEY(mode_ticfile,               CT_FILEMODE),
	CONF_KEY(mode_default,               CT_FILEMODE),
	CONF_KEY(modem_can_send_break,       CT_BOOLEAN),
	CONF_KEY(modem_dial_prefix,          CT_STRING),
	CONF_KEY(modem_dial_suffix,          CT_STRING),
	CONF_KEY(modem_dial_response,        CT_DIALRESP),
	CONF_KEY(modem_hangup_command,       CT_STRING),
	CONF_KEY(modem_port,                 CT_MODEMPORT),
	CONF_KEY(modem_reset_command,        CT_STRING),
	CONF_KEY(modem_stat_command,         CT_STRING),
	CONF_KEY(nodelist,                   CT_NODELIST),
	CONF_KEY(nodelist_directory,         CT_PATH),
	CONF_KEY(nodial_flag,                CT_STRING),
	CONF_KEY(override,                   CT_OVERRIDE),
	CONF_KEY(options,                    CT_OPTIONS),
	CONF_KEY(outbound_directory,         CT_PATH),
	CONF_KEY(password,                   CT_PASSWORD),
	CONF_KEY(phone,                      CT_STRING),
	CONF_KEY(phone_translate,            CT_TRANSLATE),
	CONF_KEY(recode_file_in,             CT_STRING),
	CONF_KEY(recode_file_out,            CT_STRING),
	CONF_KEY(recode_intro_in,            CT_STRING),
	CONF_KEY(recv_buffer_size,           CT_NUMBER),
	CONF_KEY(rescan_delay,               CT_NUMBER),
	CONF_KEY(run_after_handshake,        CT_STRING),
	CONF_KEY(run_after_session,          CT_STRING),
	CONF_KEY(run_before_session,         CT_STRING),
	CONF_KEY(session_limit_in,           CT_NUMBER),
	CONF_KEY(session_limit_out,          CT_NUMBER),
	CONF_KEY(skip_files_recv,            CT_STRING),
	CONF_KEY(status_directory,           CT_PATH),
	CONF_KEY(system_name,                CT_STRING),
	CONF_KEY(sysop_name,                 CT_STRING),
	CONF_KEY(uucp_lock_directory,        CT_PATH),
	CONF_KEY(wait_carrier_in,            CT_NUMBER),
	CONF_KEY(wait_carrier_out,           CT_NUMBER),
	CONF_KEY(zmodem_mincps_recv,         CT_CONNLIST),
	CONF_KEY(zmodem_mincps_send,         CT_CONNLIST),
	CONF_KEY(zmodem_send_dummy_pkt,      CT_BOOLEAN),
	CONF_KEY(zmodem_skip_by_pos,         CT_BOOLEAN),
	CONF_KEY(zmodem_start_block_size,	CT_NUMBER),
	CONF_KEY(zmodem_tx_window,		CT_NUMBER),
	CONF_KEY(nomail_flag,			CT_STRING),
	CONF_KEY(bind_ip,			CT_STRING),
	CONF_KEY(recieved_to_lower,		CT_BOOLEAN),
#ifdef USE_SYSLOG
	CONF_KEY(syslog_facility,		CT_NUMBER),
#endif
#ifdef DEBUG
	CONF_KEY(debug_file,			CT_STRING),
	CONF_KEY(debug_level,			CT_DEBLEVEL),
#endif
	CONF_KEY(split_inbound,			CT_BOOLEAN),
#ifdef NETSPOOL
        CONF_KEY(netspool_host,			CT_STRING),
        CONF_KEY(netspool_port,			CT_STRING),
#endif

	CONF_END()
};

static int proc_override(s_override *dest, char *value);
static int proc_options(s_options *options, char *value);
static int proc_string(s_string *dest, char *value);
static int proc_number(s_number *dest, char *value);
static int proc_filemode(s_filemode *dest, char *value);
static int proc_boolean(s_boolean *dest, char *value);
static int proc_address(s_falist *dest, char *value);
static int proc_nodelist(s_falist *dest, char *value);
static int proc_modemport(s_modemport *dest, char *value);
static int proc_domain(s_domain *dest, char *value);
static int proc_password(s_falist *dest, char *value);
static int proc_path(s_string *dest, char *value);
static int proc_dialresp(s_dialresp *dest, char *value);
static int proc_translate(s_translate *dest, char *value);
static int proc_speeddep(s_connlist *dest, char *value);
static int proc_tries(s_tries *dest, char *value);
#ifdef DEBUG
static int proc_debuglevel(s_number *dest, char *value);
#endif
static int proc_filebox(s_filebox *dest, char *value);

static int append_config_entry(s_conf_entry *conf_ent, s_cval_entry *cval_entry)
{
	s_cval_entry **ptrl;

	for( ptrl = &conf_ent->data; *ptrl; ptrl = &(*ptrl)->next );

	*ptrl = (s_cval_entry *)xmalloc(sizeof(s_cval_entry));
	memcpy(*ptrl, cval_entry, sizeof(s_cval_entry));

	return 0;
}

int proc_configline(const char *k, const char *e, const char *v)
{
	s_cval_entry temp_value;
	int rc = PROC_RC_ABORT;
	int i;
	char *copy;
	
	ASSERT(k != NULL && v != NULL );
	
	DEB((D_CONFIG, "conf_proc: key \"%s\" expr \"%s\" value \"%s\"",
		k, e, v));
	
	copy = xstrcpy(v);
	
	for( i = 0; bforce_config[i].name; i++ )
		if( !strcmp(bforce_config[i].name, k) )
			break;

	if( bforce_config[i].name )
	{
		/*
		 * Make all changes in temp_value structure
		 */
		memset(&temp_value, '\0', sizeof(s_cval_entry));
		
		switch(bforce_config[i].type) {
		case CT_ADDRESS:
			rc = proc_address(&temp_value.d.falist, copy);
			break;
		case CT_BOOLEAN:
			rc = proc_boolean(&temp_value.d.boolean, copy);
			break;
		case CT_CONNLIST:
			rc = proc_speeddep(&temp_value.d.connlist, copy);
			break;
		case CT_DIALRESP:
			rc = proc_dialresp(&temp_value.d.dialresp, copy);
			break;
		case CT_DOMAIN:
			rc = proc_domain(&temp_value.d.domain, copy);
			break;
		case CT_FILEMODE:
			rc = proc_filemode(&temp_value.d.filemode, copy);
			break;
		case CT_MODEMPORT:
			rc = proc_modemport(&temp_value.d.modemport, copy);
			break;
		case CT_NODELIST:
			rc = proc_nodelist(&temp_value.d.falist, copy);
			break;
		case CT_NUMBER:
			rc = proc_number(&temp_value.d.number, copy);
			break;
		case CT_OPTIONS:
			rc = proc_options(&temp_value.d.options, copy);
			break;
		case CT_OVERRIDE:
			rc = proc_override(&temp_value.d.override, copy);
			break;
		case CT_PATH:
			rc = proc_path(&temp_value.d.string, copy);
			break;
		case CT_PASSWORD:
			rc = proc_password(&temp_value.d.falist, copy);
			break;
		case CT_STRING:
			rc = proc_string(&temp_value.d.string, copy);
			break;
		case CT_TRANSLATE:
			rc = proc_translate(&temp_value.d.translate, copy);
			break;
		case CT_TRIES:
			rc = proc_tries(&temp_value.d.tries, copy);
			break;
#ifdef DEBUG
		case CT_DEBLEVEL:
			rc = proc_debuglevel(&temp_value.d.number, copy);
			break;
#endif
		case CT_FILEBOX:
			rc = proc_filebox(&temp_value.d.filebox, copy);
			break;
		default:
			log("internal error, unknown keyword type = %d",
					bforce_config[i].type);
			ASSERT(0);
		}

		if( rc == PROC_RC_OK || rc == PROC_RC_WARN )
		{
			if( e && *e )
			{
				temp_value.expr.expr  = xstrcpy(e);
				temp_value.expr.error = 0;
			}
			append_config_entry(&bforce_config[i], &temp_value);
		}
	}
	else
	{
		log("unknown keyword \"%s\"", k);
		rc = PROC_RC_IGNORE;
	}
	
	free(copy);

	return rc;
}

/*
 * Line format: Override <Address> <Overrides..> [Hidden <Overrides>]
 */
static int proc_override(s_override *dest, char *value)
{
	s_override **p = &dest;
	s_faddr addr;
	int rc = PROC_RC_OK;
	char *key    = NULL;
	char *arg    = NULL;
	char *n      = NULL;
	char *p_addr = NULL;
	bool neednew = FALSE;

	ASSERT(dest != NULL && value != NULL);
	
	p_addr = string_token(value, &n, NULL, 0);
	if( p_addr == NULL || *p_addr == '\0' || ftn_addrparse(&addr, p_addr, FALSE) )
		return(PROC_RC_IGNORE);
	
	memset(dest, '\0', sizeof(s_override));
	dest->addr = addr;
	
	while( key || (key = string_token(NULL, &n, NULL, 1)) )
	{
		if( (arg = string_token(NULL, &n, NULL, 1)) == NULL || *arg == '\0' )
		{
			log("no argument(s) for override \"%s\"", key);
			if( rc < PROC_RC_WARN )
				rc = PROC_RC_WARN;
			break;
		}
		
		if( neednew )
		{
			(*p) = (s_override*)xmalloc(sizeof(s_override));
			memset(*p, '\0', sizeof(s_override));
			(*p)->addr = addr;
			neednew = FALSE;
		}
		
		if( strcasecmp(key, "phone") == 0 )
		{
			if( (*p)->sPhone )
				free((*p)->sPhone);
			(*p)->sPhone = xstrcpy(arg);
			key = NULL;
		}
		else if( strcasecmp(key, "ipaddr") == 0 )
		{
			if( (*p)->sIpaddr )
				free((*p)->sIpaddr);
			(*p)->sIpaddr = xstrcpy(arg);
			key = NULL;
		}
		else if( strcasecmp(key, "run") == 0)
		{
		        if( (*p)->run )
			        free((*p)->run);
		        (*p)->run = xstrcpy(arg);
		        key = NULL;
		}
		else if( strcasecmp(key, "worktime") == 0 )
		{
			if( timevec_parse_list(&((*p)->worktime), arg) == -1 )
			{
				log("invalid work time \"%s\"", arg);
				if( rc < PROC_RC_WARN )
					rc = PROC_RC_WARN;
			}
			key = NULL;
		}
		else if( strcasecmp(key, "freqtime") == 0 )
		{
			if( timevec_parse_list(&((*p)->freqtime), arg) == -1 )
			{
				log("invalid freq time \"%s\"", arg);
				if( rc < PROC_RC_WARN )
					rc = PROC_RC_WARN;
			}
			key = NULL;
		}
		else if( strcasecmp(key, "flags") == 0 )
		{
			if( (*p)->sFlags )
				free((*p)->sFlags);
			(*p)->sFlags = xstrcpy(arg);
			key = NULL;
		}
		else if( strcasecmp(key, "hidden") == 0 )
		{
			neednew = TRUE;
			p = &((*p)->hidden);
			key = arg;
		}
		else
		{
			log("unknown keyword \"%s\" in override", key);
			if( rc < PROC_RC_WARN )
				rc = PROC_RC_WARN;
			key = arg;
		}
	}
	
	return(rc);
}

/*
 * Line format: Options <options..>
 */
static int proc_options(s_options *dest, char *value)
{
	int i, rc = PROC_RC_OK;
	char *key = NULL, *n = NULL;
	
	ASSERT(dest != NULL && value != NULL);

	for( key = string_token(value, &n, NULL, 0); key;
	     key = string_token(NULL, &n, NULL, 0) )
	{
		for( i = 0; options[i].keystr; i++ )
		{
			if( strcasecmp(key, options[i].keystr) == 0 )
			{
				if( options[i].value_no )
				{
					dest->value &= ~options[i].value_no;
					dest->mask  |= options[i].value_no;
				}
				if( options[i].value_yes )
				{
					dest->value |= options[i].value_yes;
					dest->mask  |= options[i].value_yes;
				}
#ifdef DEBUG
				if( options[i].value_no == 0 && options[i].value_yes == 0 )
					ASSERT_MSG();
#endif
				break;
			}
			else if( (strncasecmp(key, "no", 2) == 0
			      && strcasecmp(key+2, options[i].keystr) == 0)
			      || (*key == '!'
			      && strcasecmp(key+1, options[i].keystr) == 0) )
			{
				if( options[i].value_no )
				{
					dest->value |= options[i].value_no;
					dest->mask  |= options[i].value_no;
				}
				if( options[i].value_yes )
				{
					dest->value &= ~options[i].value_yes;
					dest->mask  |= options[i].value_yes;
				}
#ifdef DEBUG
				if( !options[i].value_no && !options[i].value_yes )
					ASSERT_MSG();
#endif
				break;
			}
		}
		
		if( !options[i].keystr )
		{
			log("unknown option \"%s\"", key);
			if( rc < PROC_RC_WARN )
				rc = PROC_RC_WARN;
		}
	}
	
	return(rc);
}

/*
 * Line format: <Keyword> <Any value>
 */
static int proc_string(s_string *dest, char *value)
{
	ASSERT(dest != NULL && value != NULL);

	dest->str = xstrcpy(value);
	
	return PROC_RC_OK;
}

/*
 * Line format: <Keyword> <Number>
 */
static int proc_number(s_number *dest, char *value)
{
	ASSERT(dest != NULL && value != NULL);
	
	if( !ISDEC(value) )
	{
		log("value \"%s\" isn't numeric", value);
		return(PROC_RC_IGNORE);
	}

	dest->num = atol(value);
	
	return(PROC_RC_OK);
}

/*
 * Line format: <Keyword> <Octal permissions>
 */
static int proc_filemode(s_filemode *dest, char *value)
{
	int rc = PROC_RC_OK;
	
	ASSERT(dest != NULL && value != NULL);
	
	if( !ISOCT(value) )
	{
		log("value \"%s\" isn't octal number", value);
		return(PROC_RC_IGNORE);
	}

	sscanf(value, "%o", (unsigned int *)&dest->mode);
	
	return(rc);
}

/*
 * Line format: <Keyword> <Yes|No|True|False>
 */
static int proc_boolean(s_boolean *dest, char *value)
{
	ASSERT(dest != NULL && value != NULL);
	
	if( !strcasecmp(value, "yes")
	 || !strcasecmp(value, "true") )
	{
		dest->istrue = 1;
		return(PROC_RC_OK);
	}
	else if( !strcasecmp(value, "no")
	      || !strcasecmp(value, "false") )
	{
		dest->istrue = 0;
		return(PROC_RC_OK);
	}
	
	log("unexpected argument \"%s\", must be \"Yes\" or \"No\"", value);
	
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <FTN address|Inet address>
 */
static int proc_address(s_falist *dest, char *value)
{
	s_faddr addr;
	
	ASSERT(dest != NULL && value != NULL);
	
	if( ftn_addrparse(&addr, value, FALSE) )
	{
		log("can't parse address \"%s\"", value);
		return(PROC_RC_IGNORE);
	}

	dest->addr = addr;

	return(PROC_RC_OK);
}

/*
 * Line format: <Keyword> <File name> <Address>
 */
static int proc_nodelist(s_falist *dest, char *value)
{
	s_faddr addr;
	char *n = NULL;
	char *p_name = NULL;
	char *p_addr = NULL;
	
	ASSERT(dest != NULL && value != NULL);
	
	p_name = string_token(value, &n, NULL, 1);
	p_addr = string_token(NULL, &n, NULL, 1);
	
	if( p_name && *p_name && p_addr && *p_addr )
	{
		if( ftn_addrparse(&addr, p_addr, TRUE) == 0 )
		{
			dest->addr = addr;
			dest->what = xstrcpy(p_name);
			return(PROC_RC_OK);
		}
		else
		{
			log("can't parse address \"%s\"", p_addr);
			return(PROC_RC_IGNORE);
		}
	}

	log("incorrect nodelist specification");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <domain> <path> <zone>
 */
static int proc_domain(s_domain *dest, char *value)
{
	char *n = NULL;
	char *p_domn = NULL;
	char *p_path = NULL;
	char *p_zone = NULL;
	
	ASSERT(dest != NULL && value != NULL);
	
	p_domn = string_token(value, &n, NULL, 1);
	p_path = string_token(NULL,  &n, NULL, 1);
	p_zone = string_token(NULL,  &n, NULL, 1);
	
	if( p_domn && *p_domn && p_path && *p_path && p_zone && *p_zone )
	{
		if( ISDEC(p_zone) )
		{
			dest->domain = xstrcpy(p_domn);
			dest->zone = atoi(p_zone);
			if( p_path[strlen(p_path)-1] == DIRSEPCHR )
				dest->path = xstrcpy(p_path);
			else
				dest->path = string_concat(p_path, DIRSEPSTR, NULL);
			return(PROC_RC_OK);
		}
		else
		{
			log("zone isn't numeric value in domain specification");
			return(PROC_RC_IGNORE);
		}
	}

	log("incorrect domain specification");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <port>[:<lockspeed>]
 */
static int proc_modemport(s_modemport *dest, char *value)
{
	char *p_speed = NULL;
	long speed = 0;
	
	ASSERT(dest != NULL && value != NULL);
	
	p_speed = strrchr(value, ':');
	
	if( p_speed ) {
		if( ISDEC(p_speed+1) )
		{
			p_speed[0] = '\0';
			speed = atol(p_speed+1);
		}
		else
		{
			log("incorrect modem port \"%s\": bad lock speed", value);
			return(PROC_RC_IGNORE);
		}
	}

	dest->name  = xstrcpy(value);
	dest->speed = speed;
	
	return(PROC_RC_OK);
}

/*
 * Line format: <Keyword> <Address> <Password>
 */
static int proc_password(s_falist *dest, char *value)
{
	s_faddr addr;
	char *n = NULL;
	char *p_addr = NULL;
	char *p_passwd = NULL;

	ASSERT(dest != NULL && value != NULL);
	
	p_addr = string_token(value, &n, NULL, 1);
	p_passwd = string_token(NULL, &n, NULL, 1);

	if( p_addr && *p_addr && p_passwd && *p_passwd )
	{
		if( ftn_addrparse(&addr, p_addr, FALSE) == 0 )
		{
			dest->addr = addr;
			dest->what = xstrcpy(p_passwd);
			return(PROC_RC_OK);
		}
		else
		{
			log("can't parse address \"%s\"", value);
			return(PROC_RC_IGNORE);
		}
	}

	log("incorrect password specification");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <Path>
 */
static int proc_path(s_string *dest, char *value)
{
	ASSERT(dest != NULL && value != NULL);
	
	if( value[strlen(value)-1] == DIRSEPCHR )
		dest->str = xstrcpy(value);
	else
		dest->str = string_concat(value, DIRSEPSTR, NULL);

	return(PROC_RC_OK);
}

/*
 * Line format: <Keyword> <<String1[:RC]> [String2[:RC]] ..>
 */
static int proc_dialresp(s_dialresp *dest, char *value)
{
	int i;
	char *n = NULL;
	char *p_mstr = NULL;
	char *p_type = NULL;
	char *p_retv = NULL;
	
	ASSERT(dest != NULL && value != NULL);
	
	p_mstr = string_token(value, &n, NULL, 1);
	p_type = string_token(NULL,  &n, NULL, 1);
	p_retv = string_token(NULL,  &n, NULL, 1);
	
	if( p_mstr && *p_mstr && p_type && *p_type )
	{
		if( p_retv && !ISDEC(p_retv) )
		{
			log("return code isn't numeric in modem response specification");
			return(PROC_RC_IGNORE);
		}
		
		for( i = 0; resptypes[i].str; i++ )
			if( strcasecmp(p_type, resptypes[i].str) == 0 )
				break;

		if( resptypes[i].str )
		{
			dest->mstr = xstrcpy(p_mstr);
			dest->type = resptypes[i].value;
			dest->retv = p_retv ? atoi(p_retv) : resptypes[i].retv;
			return(PROC_RC_OK);
		}

		log("unknown response type \"%s\"", p_type);
		return(PROC_RC_IGNORE);
	}

	log("incorrect modem response specification");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <Search value> <Replace value>
 */
static int proc_translate(s_translate *dest, char *value)
{
	char *n = NULL;
	char *p_find = NULL;
	char *p_repl = NULL;

	ASSERT(dest != NULL && value != NULL);
	
	p_find = string_token(value, &n, NULL, 1);
	p_repl = string_token(NULL, &n, NULL, 1);
	
	if( p_find && *p_find )
	{
		dest->find = xstrcpy(p_find);
		if( p_repl )
			dest->repl = xstrcpy(p_repl);
		return(PROC_RC_OK);
	}

	log("incorrect translation rule specification");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <Connect speed> <Number>
 */
static int proc_speeddep(s_connlist *dest, char *value)
{
	char *n = NULL;
	char *p_speed = NULL;
	char *p_value = NULL;

	ASSERT(dest != NULL && value != NULL);
	
	p_speed = string_token(value, &n, NULL, 1);
	p_value = string_token(NULL, &n, NULL, 1);
	
	if( p_speed && *p_speed && p_value && *p_value )
	{
		if( ISDEC(p_speed) && ISDEC(p_value) )
		{
			dest->speed = atoi(p_speed);
			dest->value = atoi(p_value);
			return(PROC_RC_OK);
		}
		log("non-numeric value \"%s\" or \"%s\"", p_speed, p_value);
		return(PROC_RC_IGNORE);
	}

	log("incorrect speed dependent config string");
	return(PROC_RC_IGNORE);
}

/*
 * Line format: <Keyword> <Tries number> [<Action> <Action argument(s)>]
 */
static int proc_tries(s_tries *dest, char *value)
{
	int rc = PROC_RC_OK;
	char *n = NULL;
	char *p_tries  = NULL;
	char *p_action = NULL;
	char *p_arg    = NULL;
	int action = TRIES_ACTION_UNDIALABLE;
	int arg = 0;

	ASSERT(dest != NULL && value != NULL);
	
	p_tries  = string_token(value, &n, NULL, 0);
	p_action = string_token(NULL, &n, NULL, 0);
	p_arg    = string_token(NULL, &n, NULL, 0);
	
	if( p_tries && *p_tries && ISDEC(p_tries) )
	{
		if( p_action && *p_action )
		{
			if( strcasecmp(p_action, "undialable") == 0 )
			{
				if( p_arg && *p_arg )
				{
					log("no argument needed for action 'undialable'");
					rc = PROC_RC_WARN;
				}
				action = TRIES_ACTION_UNDIALABLE;
				arg = 0;
			}
			else if( strcasecmp(p_action, "hold") == 0 )
			{
				if( p_arg && *p_arg && !ISDEC(p_arg) )
				{
					log("non-numeric argument for action 'hold'");
					return PROC_RC_IGNORE;
				}
				else if( p_arg == NULL )
				{
					log("no argument for action 'hold'");
					return PROC_RC_IGNORE;
				}
				action = TRIES_ACTION_HOLDSYSTEM;
				arg = atoi(p_arg);
			}
			else if( strcasecmp(p_action, "holdall") == 0 )
			{
				if( p_arg && *p_arg && !ISDEC(p_arg) )
				{
					log("non-numeric argument for action 'holdall'");
					return PROC_RC_IGNORE;
				}
				else if( p_arg == NULL )
				{
					log("no argument for action 'holdall'");
					return PROC_RC_IGNORE;
				}
				action = TRIES_ACTION_HOLDALL;
				arg = atoi(p_arg);
			}
			else
			{
					log("unknown action '%s'", p_action);
					return PROC_RC_IGNORE;
			}
		}
		dest->tries  = atoi(p_tries);
		dest->action = action;
		dest->arg    = arg;
		return(rc);
	}

	log("non-numeric tries number \"%s\"", p_tries);
	return(PROC_RC_IGNORE);
}

/*
 *  Line format: DebugLevel <Level> [<Level>]..
 */
#ifdef DEBUG
static int proc_debuglevel(s_number *dest, char *value)
{
	int rc = PROC_RC_OK;
	long deblevel = 0L;

	ASSERT(dest != NULL && value != NULL);
	
	if( debug_parsestring(value, &deblevel) ) rc = PROC_RC_WARN;
	
	dest->num = deblevel;

	return(rc);
}
#endif

/*
 * Line format: <Keyword> <Directory> <Address>
 */
static int proc_filebox(s_filebox *dest, char *value)
{
	s_faddr addr;
	char *n = NULL;
	char *p_path = NULL;
	char *p_addr = NULL;
	char *p_flav = NULL;
	
	ASSERT(dest != NULL && value != NULL);
	
	p_path = string_token(value, &n, NULL, 1);
	p_addr = string_token(NULL, &n, NULL, 1);
	p_flav = string_token(NULL, &n, NULL, 1);
	
	if( p_path && *p_path && p_addr && *p_addr )
	{
		if( ftn_addrparse(&addr, p_addr, TRUE) == 0 )
		{
			int flavor = FLAVOR_HOLD;
			int rc = PROC_RC_OK;
			
			if( !is_directory(p_path) )
			{
				log("filebox directory \"%s\" doesn't exist", p_path);
				rc = PROC_RC_WARN;
			}
			if( p_flav && *p_flav )
			{
				if( strcasecmp(p_flav, "hold") == 0 )
					flavor = FLAVOR_HOLD;
				else if( strcasecmp(p_flav, "normal") == 0 )
					flavor = FLAVOR_NORMAL;
				else if( strcasecmp(p_flav, "crash") == 0 )
					flavor = FLAVOR_CRASH;
				else if( strcasecmp(p_flav, "immediate") == 0 )
					flavor = FLAVOR_IMMED;
				else
				{
					log("unknown filebox flavor \"%s\"", p_flav);
					rc = PROC_RC_WARN;
				}
			}
			dest->addr = addr;
			if( p_path[strlen(p_path)-1] == DIRSEPCHR )
				dest->path = xstrcpy(p_path);
			else
				dest->path = string_concat(p_path, DIRSEPSTR, NULL);
			dest->flavor = flavor;
			return rc;
		}
		else
		{
			log("can't parse address \"%s\"", p_addr);
			return PROC_RC_IGNORE;
		}
	}

	log("incorrect filebox specification");
	return PROC_RC_IGNORE;
}
