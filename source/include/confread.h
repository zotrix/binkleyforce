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

#ifndef _CONFREAD_H_
#define _CONFREAD_H_

#include "util.h"

/* Constatnts for gotfile switches */
#define GOTFILE_SW_SRIF		0x01
#define GOTFILE_SW_IMMED	0x01
#define GOTFILE_SW_ONLINE	0x01

#define MAXINCLUDELEVEL	10

#define PROC_RC_OK	0	/* Successfully processed string */
#define PROC_RC_WARN	1	/* Successfully, but not all so good */
#define PROC_RC_IGNORE	2	/* Line was ignored, but we can continue */
#define PROC_RC_ABORT	3	/* Incorrect line and we can not continue */

/*
 *  Options (from/for "options" keyword)
 */
#define OPTIONS_NO_ZMODEM	0x00000001L
#define OPTIONS_NO_ZEDZAP	0x00000002L
#define OPTIONS_NO_DIRZAP	0x00000004L
#define OPTIONS_NO_JANUS	0x00000008L
#define OPTIONS_NO_HYDRA	0x00000010L
#define OPTIONS_NO_BINKP	0x00000020L
#define OPTIONS_NO_TCP		0x00000040L
#define OPTIONS_NO_CHAT		0x00000080L
#define OPTIONS_NO_FTS1		0x00000100L
#define OPTIONS_NO_YOOHOO	0x00000200L
#define OPTIONS_NO_EMSI		0x00000400L
#define OPTIONS_NO_EMSI_II	0x00000800L
#define OPTIONS_NO_FREQS	0x00001000L
#define OPTIONS_MAILONLY	0x00002000L
#define OPTIONS_HOLDXT		0x00004000L
#define OPTIONS_HOLDREQ		0x00008000L
#define OPTIONS_HOLDALL		0x00010000L
#define OPTIONS_HOLDHOLD	0x00020000L
#define OPTIONS_NO_PICKUP	0x00040000L
#define OPTIONS_NO_RH1		0x00080000L
#define OPTIONS_NO_INTRO	0x00100000L

#define RESPTYPE_CONNECT	0x0000L
#define RESPTYPE_BUSY		0x0001L
#define RESPTYPE_NOCARRIER	0x0002L
#define RESPTYPE_NODIALTONE	0x0004L
#define RESPTYPE_NOANSWER	0x0008L
#define RESPTYPE_ERROR		0x0010L

#define TRIES_ACTION_UNDIALABLE	0x01
#define TRIES_ACTION_HOLDSYSTEM	0x02
#define TRIES_ACTION_HOLDALL	0x04

/*
 * Add new entry to the end of list
 */
#define LIST_NEWENT(newp,list,type) \
	for( newp = list; *newp; newp = &((*newp)->next) ); \
	*newp = (type*)xmalloc(sizeof(type)); \
	memset(*newp, '\0', sizeof(type));

typedef struct expr {
	bool error;		/* Incorrect expression. Don't try twice! */
	char *expr;		/* Pointer to the expression string */
} s_expr;

/*
 *  Storage structures for base config components
 */
typedef struct string {
	char *str;
} s_string;

typedef struct number {
	long num;
} s_number;

typedef struct boolean {
	bool istrue;
} s_boolean;

typedef struct falist {
	s_faddr addr;
	char *what;
	struct falist *next;
} s_falist;

typedef struct domain {
	char *domain;
	char *path;
	int zone;
} s_domain;

typedef struct translate {
	char *find;
	char *repl;
} s_translate;

typedef struct connlist {
	long speed;
	long value;
} s_connlist;

typedef struct dialresp {
	char *mstr;	/* Modem string/substring */
	int type;	/* Response meaning (for mailer) */
	int retv;	/* Force this return code */
} s_dialresp;

typedef struct filemode {
	mode_t mode;
} s_filemode;

typedef struct modemport {
	long speed;
	char *name;
} s_modemport;

typedef struct tries {
	int tries;
	int action;
	int arg;
} s_tries;

typedef struct override {
	s_faddr addr;			/* Overrides for this address        */
	char *sIpaddr;
	char *sPhone;
	char *sFlags;
	s_timevec worktime;
        s_timevec freqtime;
        char *run;
	struct override *hidden;	/* Hidden lines list                 */
} s_override;

typedef struct options {
	unsigned long value;
	unsigned long mask;	/* bit[k] == 1 if bit[k] defined in value    */
} s_options;

typedef struct filebox {
	s_faddr addr;
	char *path;
	int flavor;
	struct falist *next;
} s_filebox;

typedef enum {
	cf_address,
	cf_amiga_outbound_directory,
	cf_binkp_timeout,
	cf_daemon_circle,
	cf_daemon_circle_crash,
	cf_daemon_circle_direct,
	cf_daemon_circle_immed,
	cf_daemon_circle_modem,
	cf_daemon_circle_normal,
	cf_daemon_circle_rescan,
	cf_daemon_maxclients_modem,
	cf_daemon_maxclients_tcpip,
	cf_daemon_pid_file,
	cf_delay_files_recv,
	cf_delay_files_send,
	cf_disable_aka_matching,
	cf_domain,
	cf_emsi_FR_time,
	cf_emsi_OH_time,
	cf_emsi_slave_sends_nak,
	cf_filebox,
	cf_filebox_directory,
	cf_flags,
	cf_flo_translate,
	cf_freq_alias_list,
	cf_freq_dir_list,
	cf_freq_ignore_masks,
	cf_freq_limit_number,
	cf_freq_limit_size,
	cf_freq_limit_time,
	cf_freq_min_speed,
	cf_freq_srif_command,
	cf_hide_our_aka,
	cf_history_file,
	cf_hydra_options,
	cf_hydra_mincps_recv,
	cf_hydra_mincps_send,
	cf_hydra_tx_window,
	cf_hydra_rx_window,
	cf_inbound_directory,
	cf_location,
	cf_log_file,
	cf_log_file_daemon,
	cf_max_speed,
	cf_maxtries,
	cf_maxtries_nodial,
	cf_maxtries_noansw,
	cf_maxtries_noconn,
	cf_maxtries_hshake,
	cf_maxtries_sessions,
	cf_min_cps_recv,
	cf_min_cps_send,
	cf_min_cps_time,
	cf_min_free_space,
	cf_min_speed_in,
	cf_min_speed_out,
	cf_mode_netmail,
	cf_mode_arcmail,
	cf_mode_request,
	cf_mode_ticfile,
	cf_mode_default,
	cf_modem_can_send_break,
	cf_modem_dial_prefix,
	cf_modem_dial_suffix,
	cf_modem_dial_response,
	cf_modem_hangup_command,
	cf_modem_port,
	cf_modem_reset_command,
	cf_modem_stat_command,
	cf_nodelist,
	cf_nodelist_directory,
	cf_nodial_flag,
	cf_override,
	cf_options,
	cf_outbound_directory,
	cf_password,
	cf_phone,
	cf_phone_translate,
	cf_recode_file_in,
	cf_recode_file_out,
	cf_recode_intro_in,
	cf_recv_buffer_size,
	cf_rescan_delay,
	cf_run_after_handshake,
	cf_run_after_session,
	cf_run_before_session,
	cf_session_limit_in,
	cf_session_limit_out,
	cf_skip_files_recv,
	cf_status_directory,
	cf_system_name,
	cf_sysop_name,
	cf_uucp_lock_directory,
	cf_wait_carrier_in,
	cf_wait_carrier_out,
	cf_zmodem_mincps_recv,
	cf_zmodem_mincps_send,
	cf_zmodem_send_dummy_pkt,
	cf_zmodem_skip_by_pos,
	cf_zmodem_start_block_size,
	cf_zmodem_tx_window,
	cf_nomail_flag,
	cf_bind_ip,
	cf_recieved_to_lower,
#ifdef USE_SYSLOG
	cf_syslog_facility,
#endif
#ifdef DEBUG
	cf_debug_file,
	cf_debug_level,
#endif
	cf_split_inbound,
	BFORCE_NUMBER_OF_KEYWORDS
} bforce_config_keyword;

typedef struct cval_entry {
	s_expr expr;
	union {
		s_falist     falist;
		s_domain     domain;
		s_override   override;
		s_options    options;
		s_translate  translate;
		s_connlist   connlist;
		s_number     number;
		s_boolean    boolean;
		s_string     string;
		s_modemport  modemport;
		s_dialresp   dialresp;
		s_tries      tries;
		s_filemode   filemode;
		s_filebox    filebox;
	} d;
	struct cval_entry *next;
} s_cval_entry;

typedef struct conf_entry {
	char *name;
	enum {
		CT_ADDRESS,
		CT_BOOLEAN,
		CT_CONNLIST,
		CT_DIALRESP,
		CT_DOMAIN,
		CT_FILEMODE,
		CT_MODEMPORT,
		CT_NODELIST,
		CT_NUMBER,
		CT_OPTIONS,
		CT_OVERRIDE,
		CT_PATH,
		CT_PASSWORD,
		CT_STRING,
		CT_TRANSLATE,
		CT_TRIES,
		CT_DEBLEVEL,
		CT_FILEBOX
	} type;
	s_cval_entry *data;
	bforce_config_keyword real_key;
} s_conf_entry;

extern s_conf_entry bforce_config[];

/* conf_proc.c */
int proc_configline(const char *k, const char *e, const char *v);

/* conf_deinit.c */
void deinit_conf(void);
void deinit_cval_entry(s_cval_entry *dest, bforce_config_keyword type);
void deinit_dialresp(s_dialresp *dest);
void deinit_domain(s_domain *dest);
void deinit_expr(s_expr *dest);
void deinit_falist(s_falist *dest);
void deinit_modemport(s_modemport *dest);
void deinit_override(s_override *dest);
void deinit_string(s_string *dest);
void deinit_translate(s_translate *dest);

/* conf_read.c */
const char *conf_getconfname(void);
int conf_postreadcheck(void);
int conf_readpasswdlist(s_falist **pwdlist, char *fname);
int conf_readconf(const char *confname, int inclevel);
#ifdef DEBUG
void log_overridelist(s_override *subst);
void log_options(s_options *opt);
#endif

/* conf_get.c */
s_cval_entry *conf_first(bforce_config_keyword keyword);
s_cval_entry *conf_next(s_cval_entry *ptrl);
bool conf_boolean(bforce_config_keyword keyword);
mode_t conf_filemode(bforce_config_keyword keyword);
long conf_options(bforce_config_keyword keyword);
long conf_connlist(bforce_config_keyword keyword, long speed);
char *conf_string(bforce_config_keyword keyword);
long conf_number(bforce_config_keyword keyword);
s_override *conf_override(bforce_config_keyword keyword, s_faddr addr);
s_tries *conf_tries(bforce_config_keyword keyword);

/* expression.y */
bool eventexpr(s_expr *expr);

#endif /* _CONFREAD_H_ */

