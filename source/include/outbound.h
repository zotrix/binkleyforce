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

#ifndef _OUTBOUND_H_
#define _OUTBOUND_H_

#include "nodelist.h"

#define FLAVOR_NONE		0x000
#define FLAVOR_HOLD		0x001
#define FLAVOR_NORMAL		0x002
#define FLAVOR_DIRECT		0x004
#define FLAVOR_IMMED		0x008
#define FLAVOR_CRASH		0x010

#define ACTION_NOTHING		0x000	/* Do nothing with file after ses.   */
#define ACTION_UNLINK		0x001	/* Unlink file if send successful    */
#define ACTION_TRUNCATE		0x002	/* Truncate file if send successful  */
#define ACTION_FORCEUNLINK	0x004	/* Unlink file in any case after ses.*/
#define ACTION_ACKNOWLEDGE	0x008 	/* Report success to netspool server */

#define TYPE_UNKNOWN		0x000
#define TYPE_NETMAIL		0x001
#define TYPE_ARCMAIL		0x002
#define TYPE_TICFILE		0x004
#define TYPE_FILEECHO		0x008
#define TYPE_FILEBOX		0x010
#define TYPE_FROMFLO		0x020
#define TYPE_REQUEST		0x040
#define TYPE_REQANSW		0x080	/* Our answer on incoming file req.  */
#define TYPE_FLOFILE		0x100
#define TYPE_ASONAME		0x200	/* File with AmigaDOS Style Outbound */
                    		     	/* name (e.g. 2.5020.1398.11.out)    */

#define STATUS_WILLSEND		0
#define STATUS_SKIP		1
#define STATUS_SENDING		2
#define STATUS_SENT		3

#define QUEUE_SORT_ADDRESS	0x01
#define QUEUE_SORT_SIZE	0x02
#define QUEUE_SORT_REVERSE	0x04

typedef struct {
	FILE *fp;
	char  att_path[BFORCE_MAX_PATH+1];
	int   att_action;
	int   att_offs; /* offset of the curent line */
} s_FLO;

enum { OUTB_TYPE_BSO,
	   OUTB_TYPE_DOMAIN,
       OUTB_TYPE_ASO,
       OUTB_TYPE_FBOX };

typedef struct outbound_callback_data {
	int (*callback)(struct outbound_callback_data *);
	void *dest;
	s_faddr addr;
	char *path;
	int flavor; /* Force this flavor for file */
	unsigned short type;
} s_outbound_callback_data;

typedef struct filelist {
	struct filelist *next;
	char *fname;
	size_t size;
	unsigned short type;
	unsigned short status;
	unsigned short action;
	unsigned short flavor;
	short flodsc;
} s_filelist;

typedef struct flofile {
	char *fname;
	short flavor;
	time_t mtime;
} s_flofile;

typedef struct fsqueue {
	s_flofile  *flotab;
	s_filelist *fslist;
	int flonum;		/* Number of items in .?LO files table */
} s_fsqueue;

#define TRIES_RESET -1 /* Use this macro to reset tries counter  */
#define HOLD_RESET   1 /* Use this macro to reset hold timer  */

typedef struct sess_stat {
	bool undialable;
	int  tries;
	int  tries_noconn;
	int  tries_nodial;
	int  tries_noansw;
	int  tries_hshake;
	int  tries_sessns;
	unsigned long hold_until;
	unsigned long hold_freqs;
	unsigned long last_success;
	unsigned long last_success_in;
	unsigned long last_success_out;
} s_sess_stat;

typedef struct sysentry {
	s_node node;
	bool lookedup;
const	s_override *overrides; /* List of all overrides */
const	s_override *lineptr;   /* List of all overrides */
	s_sess_stat stat;
	int  line;              /* Current line number */
	bool tcpip;             /* Call to the current line via TCP/IP? */
	bool busy;              /* Address is busy [in an another queue, etc.] */
	time_t lastcall;        /* Last call _finish_ time */
	time_t mailage;         /* The oldest file modification time */
	size_t netmail_size;
	size_t arcmail_size;
	size_t request_size;
	size_t files_size;
	int  flavors;
	int  types;
} s_sysentry;

typedef struct sysqueue {
	s_sysentry *systab;
	int sysnum;
	time_t holduntil;
	int current_modem;
	int current_tcpip;
	int next_modem;
	int next_tcpip;
} s_sysqueue;

typedef struct exttab {
const	char *ext;
	int type;
	int flavor;
	int action;
} s_exttab;

extern s_exttab outtab[];
extern s_exttab exttab[];
extern int (*sysqueue_add_callback)(s_sysentry *);
extern int (*sysqueue_rem_callback)(s_sysentry *);
extern bool sysqueue_dont_count_sizes;

/* outb_bsy.c */
int out_bsy_check(s_faddr addr);
#ifdef BFORCE_USE_CSY
int out_bsy_lock(s_faddr addr, bool csy_locks);
#else
int out_bsy_lock(s_faddr addr);
#endif
int out_bsy_unlock(s_faddr addr);
int out_bsy_unlockall(void);

/* outb_flo.h */
s_FLO *flo_open(const char *path, const char *mode);
int    flo_next(s_FLO *FLO);
int    flo_mark_sent(s_FLO *FLO);
int    flo_eof(s_FLO *FLO);
int    flo_close(s_FLO *FLO);
bool   out_flo_isempty(const char *path);
int    out_flo_marksent(const char *fname, const char *floname);
int    out_flo_getsizes(const char *floname, size_t *arcmail, size_t *files);
int    out_flo_unlinkempty(const s_flofile *flotab, int flonum);

/* outb_getname.c */
char *out_getname_4d(s_faddr addr);
char *out_getname_domain(s_faddr addr);
char *out_getname_amiga(s_faddr addr);

/* outb_queue.c */
int out_filetype(const char *fname);
int out_handle_fsqueue(s_outbound_callback_data *callback);
void deinit_filelist(s_filelist *filelist);
void deinit_fsqueue(s_fsqueue *q);
#ifdef DEBUG
void log_filelist(const s_filelist *filelist);
void log_fsqueue(const s_fsqueue *q);
#endif

/* outb_main.c */
int out_get_root(const char *outbound, char **out_root, char **out_main);
int out_parse_name_aso(s_faddr *addr, const char *name);
int out_scan(s_outbound_callback_data *callback, const s_falist *fa_list);

/* outb_sysqueue.c */
void out_reset_trycounters(s_sysentry *syst);
void out_reset_sysqueue(s_sysqueue *q);
void out_sysqueue_sort(s_sysqueue *q, int opts);
void out_remove_from_sysqueue(s_sysqueue *q, int pos);
int out_handle_sysqueue(s_outbound_callback_data *callback);
void deinit_sysqueue(s_sysqueue *q);
#ifdef DEBUG
void log_sysqueue(const s_sysqueue *q);
#endif

#endif
