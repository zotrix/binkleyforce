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

#ifndef _S_MAIN_H_
#define _S_MAIN_H_

#define CALLOPT_FORCE	0x01
#define CALLOPT_INET	0x02

#include "io.h"
#include "nodelist.h"
#include "outbound.h"
#include "prot_common.h"

typedef enum session {
	SESSION_UNKNOWN,
	SESSION_FTSC,
	SESSION_YOOHOO,
	SESSION_EMSI,
	SESSION_BINKP
} e_session;

typedef enum protocol {
	PROT_NOPROT,
	PROT_XMODEM,
	PROT_ZMODEM,
	PROT_ZEDZAP,
	PROT_DIRZAP,
	PROT_JANUS,
	PROT_HYDRA,
	PROT_TCP,
	PROT_BINKP
} e_protocol;

typedef enum reqstat {
	REQS_DISABLED,		/* Freqs not supported by the system         */
	REQS_ALLOW,		/* We will process file requests             */
	REQS_NOTALLOW		/* Freqs TEMPORARY disabled for any reason   */
} e_reqstat;

#define HRC_OK          0  /* No errors                                  */
#define HRC_LOW_SPEED   1  /* Connect speed too low (shown it in EMSI)   */
#define HRC_BAD_PASSWD  2  /* Pasword failure (shown it in EMSI)         */
#define HRC_NO_ADDRESS  3  /* Expected address was not presented         */
#define HRC_NO_PROTOS   4  /* No common protocols :(                     */
#define HRC_BUSY        5  /* All remote AKAs are busy                   */
#define HRC_FATAL_ERR   6  /* Any other fatal handshake error            */
#define HRC_TEMP_ERR    7  /* Any other temporary handshake error        */
#define HRC_OTHER_ERR   8  /* Any other error (e.g. timeout, NO CARRIER) */

typedef struct sysaddr {
	s_faddr addr;
	bool busy;
	bool good;
	int  flags;
} s_sysaddr;

typedef struct {
	char passwd[128];
	char challenge[128];
	int  challenge_length;
	bool cram_auth;
} s_session_passwd;

typedef struct sendopts {
	int fnc:1,        /* Convert outgoing file names to 8+3 format */
	    holdreq:1,    /* Hold .REQ files? (file requests) */
	    holdhold:1,   /* Hold files with HOLD flavor? */
	    holdfiles:1,  /* Hold all files except netmail + arcmail */
	    holdxt:1,     /* Hold all except netmail? */
	    holdall:1,    /* Hold ALL trafic!? */
		hydraRH1;     /* Hydra RH1 mode (EMSI only) */
} s_sendopts;

/*
 * Handhake protocols API
 */
typedef struct handshake_protocol
{
	/*
	 * 1. Main pointers
	 */
	char *verbal_name;
	char *author;
	char *standard;
	char *remote_data;
	char *local_data;
	e_protocol protocol;

	void (*init)(struct handshake_protocol *THIS);
	void (*deinit)(struct handshake_protocol *THIS);
	int (*incoming_session)(struct handshake_protocol *THIS);
	int (*outgoing_session)(struct handshake_protocol *THIS);
	
	/*
	 * 2. Remote system information extract methods
	 */
	s_faddr* (*remote_address)(struct handshake_protocol *THIS);
	char* (*remote_password)(struct handshake_protocol *THIS);
	char* (*remote_sysop_name)(struct handshake_protocol *THIS);
	char* (*remote_system_name)(struct handshake_protocol *THIS);
	char* (*remote_location)(struct handshake_protocol *THIS);
	char* (*remote_phone)(struct handshake_protocol *THIS);
	char* (*remote_flags)(struct handshake_protocol *THIS);
	char* (*remote_mailer)(struct handshake_protocol *THIS);
	int (*remote_traffic)(struct handshake_protocol *THIS, s_traffic *dest);

	/*
	 * 3. Local system information extract methods
	 */
	s_faddr* (*local_address)(struct handshake_protocol *THIS);
	char* (*local_password)(struct handshake_protocol *THIS);
} s_handshake_protocol;

struct state
{
	s_node     node;
const	s_modemport *modemport;
	char       *linename;      /* Verbal line name (i.e. ttyS1, tcpip, etc.) */
	time_t      start_time;    /* Session start time (e.g. connect time) */
	e_tcpmode   tcpmode;       /* Current mode to work via TCP/IP */
	e_session   session;       /* handshake type (EMSI, YooHoo, etc.) */
	bool        valid;         /* This structure contain real information */
	bool        caller;        /* Outgoing session */
	bool        protected;     /* Password protected session? */
	bool        listed;        /* Session with listed system? */
	bool        inet;          /* We are working via TCP/IP now */
	int         session_rc;    /* Session return code */
	s_sendopts  sopts;         /* Our send options (hold arcmail, etc.) */
	e_reqstat   reqstat;       /* Our file request processor status */
	s_override  override;
	s_sess_stat sess_stat;     /* Try counters, hold timers, etc. */
	long        connspeed;
	long        minspeed;
	char       *cidstr;        /* Caller Phone/ID when we are answering */
	char       *peername;      /* Remote peer name than using TCP/IP */
	long        peerport;      /* Remote port number, we are connected to */
	char       *connstr;
	char       *inbound;       /* Inbound directory for this session */
	char       *tinbound;      /* Temporary inbound for this session */
	s_traffic   traff_send;    /* Expected outgoing traffic */
	s_traffic   traff_recv;    /* Expected incoming traffic */
	
	s_handshake_protocol *handshake;
	
	s_falist *mailfor;         /* Remote valid addresses */
	s_fsqueue queue;           /* Send files queue */
};
typedef struct state s_state;

/* s_common.c */
int   session_get_local_address(s_faddr *addr);
int   session_get_remote_address(s_faddr *addr);
int   session_get_remote_sysop(char **dest);
int   session_get_remote_systname(char **const dest);
int   session_get_remote_mailer(char **const dest);
int   session_get_remote_phone(char **const dest);
int   session_get_remote_location(char **const dest);
int   session_is_remote_protected(void);
int   session_is_remote_listed(void);
int   session_get_callerid(char **const dest);
int   session_get_connect(char **const dest);
int   session_run_command(const char *execstr);
int   override_get(s_override *dest, s_faddr addr, int line);
void  init_state(s_state *pstate);
void  deinit_state(s_state *pstate);

/* s_init.c */
int   session_init_outgoing();
int   session_init_incoming();

/* s_main.c */
extern s_state state;

s_faddr *session_get_bestaka(s_faddr addr);
int   session_addrs_lock(s_sysaddr *addrs, int anum);
int   session_addrs_add(s_sysaddr **addrs, int *anum, s_faddr addr);
int   session_addrs_check(s_sysaddr *addrs, int anum, const char *passwd,
                          const char *challenge, int challenge_length);
int   session_addrs_to_falist(s_sysaddr *addrs, int anum, s_falist **dest);
int   session_addrs_check_genuine(s_sysaddr *addrs, int anum, s_faddr expected);
int   session_check_speed(void);
int   session_check_addr(s_faddr addr);
int   session_get_password(s_faddr addr, char *buffer, size_t buflen);
int   session_remote_lookup(s_sysaddr *addrs, int anum);
void  session_remote_log_status(void);
int   session_check_password(s_faddr addr, const char *passwd);
int   session_set_inbound(void);
void  session_set_freqs_status(void);
void  session_set_send_options(void);
void  session_set_send_files(void);
int   session_create_files_queue(s_sysaddr *addrs, int anum);
int   session_traffic_set_incoming(s_traffic *dest);
int   session_traffic_set_outgoing(s_traffic *dest);
void  session_traffic(void);
void  session_update_history(s_traffic *send, s_traffic *recv, int rc);
int   session(void);

/* sess_call.c */
int   call_system(s_faddr addr, const s_bforce_opts *opts);
int   call_system_modem(void);
int   call_system_tcpip(int callwith);

/* sess_answ.c */
int   answ_system(e_session type, char *connstr, int inetd);

/* sess_stat.c */
void  session_stat_reset_counters(s_sess_stat *stat);
int   session_stat_get(s_sess_stat *stat, s_faddr *addr);
int   session_stat_apply_diff(s_faddr addr, s_sess_stat stat);
int   session_stat_update(s_faddr *addr, s_sess_stat *stat, bool caller, int rc);

#endif
