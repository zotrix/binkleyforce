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

#ifndef _P_COMMON_H_
#define _P_COMMON_H_

#include "outbound.h"

/* File transfer protocols return codes */
#define PRC_NOERROR         0       /* No comments :) */
#define PRC_ERROR           1       /* I/O error occured while snd./rcv. */
#define PRC_REMOTEABORTED   2       /* "ABORT" initiated by remote */
#define PRC_LOCALABORTED    3       /* We got SIGINT/SIGTERM? */
#define PRC_CPSTOOLOW       4       /* Cps was so low.. :( */
#define PRC_STOPTIME        5       /* Aborted due to the time limits */

/* Send/Recv file status values */
#define FSTAT_PROCESS  1
#define FSTAT_WAITACK  2
#define FSTAT_SUCCESS  3
#define FSTAT_SKIPPED  4
#define FSTAT_REFUSED  5

typedef struct traffic {
	int    netmail_num;    /* Number of netmail packets */
	int    netmail_time;   /* Netmail packets sending/receiving time */
	size_t netmail_size;   /* Size of netmail packtes */
	int    arcmail_num;    /* Dito.. */
	int    arcmail_time;
	size_t arcmail_size;
	int    files_num;
	int    files_time;
	size_t files_size;
	int    freqed_num;
	int    freqed_time;
	size_t freqed_size;
} s_traffic;

/*
 * Information on currently receiveing/transmitting file
 */
typedef struct finfo {
	FILE  *fp;              /* File descriptor of current file */
	char  *local_name;      /* Local file name that is used at our side */
	char  *net_name;        /* File name as it known to the remote side */
	char  *fname;           /* Local file name with full path */
	time_t start_time;      /* Start of current file rx/tx transactions  */
	time_t mod_time;        /* File last modification time */
	mode_t mode;
	size_t bytes_total;     /* File size */
	size_t bytes_sent;
	size_t bytes_received;
	size_t bytes_skipped;   /* crash recovery */
	int    eofseen;         /* end of file seen */
	int    status;
	int    action;
	int    type;
	int    flodsc;          /* Index number in flo files table */
} s_finfo;

/*
 * Transfer protocol related structure
 */
typedef struct protinfo {
	/* ----------------------------------------------------------------- */
	/* Variables defined before protocol starting                        */
	/* ----------------------------------------------------------------- */
	bool   reqs_only;       /* Send only .REQ files (for Hydra RH1 mode) */
	time_t stop_time;       /* Stop transfer at this time */
	int    freq_timelimit;  /* Abort transfer if send freqs longer this */
	int    min_tx_cps;      /* Minimal transmite speed */
	int    min_rx_cps;      /* Minimal receive speed */
	int    min_cps_time;    /* Abort transfer if minimal bps stay during
	                           this time period */
	char  *buffer;          /* Ptr. to buffer for buffered file saving */
	size_t buflen;          /* Buffer size */
	/* ----------------------------------------------------------------- */
	/* All variables below used by misc protocol routines                */
	/* ----------------------------------------------------------------- */
	s_filelist *filelist;   /* List of files.. It is our answer on .req! */
	int    tx_cps;          /* Current transmite CPS */
	int    rx_cps;          /* Current receive CPS */
	int    send_freqtime;   /* Time we spend at sending REQed files */
	time_t tx_low_cps_time; /* If CPS lower min_tx_cps it is time how
	                           long it lasts */
	time_t rx_low_cps_time; /* Same, but for transmite CPS */
	time_t start_time;      /* Time transfer started at */

	s_traffic traffic_sent;
	s_traffic traffic_rcvd;
	size_t send_left_size;
	size_t recv_left_size;
	int    send_left_num;
	int    recv_left_num;
	
	s_finfo *send;
	s_finfo *recv;
	s_finfo *sentfiles;
	s_finfo *rcvdfiles;
	int    n_sentfiles;
	int    n_rcvdfiles;
} s_protinfo;

extern const char *Protocols[]; /* Protocol names */

int   p_tx_fopen(s_protinfo *pi);
int   p_tx_fclose(s_protinfo *pi);
int   p_tx_readfile(char *buf, size_t sz, s_protinfo *pi);
int   p_rx_writefile(const char *buf, size_t sz, s_protinfo *pi);
int   p_compfinfo(s_finfo *finf, const char *fn, size_t sz, time_t tm);
int   p_rx_fopen(s_protinfo *pi, char *f_name, size_t f_size, time_t f_modtime, mode_t f_mode);
int   p_rx_fclose(s_protinfo *pi);
int   p_info(s_protinfo *pi, int bidir);
void  prot_update_traffic(s_protinfo *pi, const s_fsqueue *q);
void  p_log_txrxstat(const s_protinfo *pi);
void  p_session_cleanup(s_protinfo *pi, bool success);
void  p_shortname(char *name);
char *prot_unique_name(char *dirname, char *fname, int type);
void  p_checkname(char *name);
void  deinit_finfo(s_finfo *fi);
void  init_protinfo(s_protinfo *pi, bool caller);
void  deinit_protinfo(s_protinfo *pi);
char *p_convfilename(const char *origname, int type);
char *p_gettmpname(const char *inbound, char *fname, s_faddr addr, size_t sz, time_t tm);

#endif
