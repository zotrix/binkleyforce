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

#ifndef _DAEMON_H_
#define _DAEMON_H_

#include "outbound.h"

/*
 * Branch information
 */
typedef struct {
	s_faddr addr;
	bool tcpip;
	char *portname;
	bool wait;      /* It is like a zombie! */
	pid_t pid;      /* Branch PID, returned by fork() call */
	time_t start;
	time_t stop;    /* We should stop (kill) branch at this time */
	int rc;         /* Process return code (one of BFERR values) */
} s_daemon_branch;

/*
 * Daemon queue sates
 */
typedef enum {
	DQ_CallSystem,
	DQ_GetNextSystem,
	DQ_Idle
} DQ_State;

/*
 * Daemon states
 */
typedef enum {
	DM_Start,
	DM_Restart,
	DM_ProcessQueues,
	DM_Usr1,
	DM_Usr2,
	DM_Shutdown
} DM_State;

/*
 * Daemon queue
 */
typedef struct {
	s_sysqueue *q;       /* Pointer to the systems queue (shared) */
	int current;
	int circle;
	bool tcpip;          /* Queue for TCP/IP systems? */
	DQ_State state;
} s_daemon_queue;

extern int max_tcpip;
extern int max_modem;
extern int tcpip_clients;
extern int modem_clients;

/* daemon.c */
int daemon_run(const char *confname, const char *incname, bool quit);

/* daemon_branch.c */
bool daemon_branch_exist(s_faddr addr);
void daemon_branch_check_stop_timers(void);
int  daemon_branch_exit(pid_t pid, int rc);
int  daemon_branch_get_first_waiting(void);
s_daemon_branch *daemon_branch_get_pointer(int pos);
void daemon_branch_add(s_faddr addr, pid_t pid, bool tcpip, const char *pname);
void daemon_branch_remove(int pos);
int  daemon_branch_number(void);
void daemon_branch_deinit(void);

/* daemon_call.c */
int  daemon_call(s_sysentry *syst);

/* daemon_lines.c */
int  daemon_line_add(const char *name, int type);
void daemon_line_hold(const char *name, int holdtime);
bool daemon_line_isready(const char *name);
void daemon_lines_deinit(void);

#endif /* _DAEMON_H_ */
