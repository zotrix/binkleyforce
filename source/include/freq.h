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

#ifndef _FREQ_H_
#define _FREQ_H_

#include "session.h"
#include "outbound.h"

/*
 *  List of our available for FREQing resources
 */
typedef struct frlist {
	struct frlist *next;
	char *magic;
	char *path;
	char *passwd;
	char *expr;		/* process only if expression is true */
} s_frlist;

/*
 *  List of requested files (from requester)
 */
typedef struct reqlist {
	struct reqlist *next;
	char *fmask;
	char *passwd;
	time_t newer;
	time_t older;
	int skip;
} s_reqlist;

/*
 *  Keep all FREQ processor-important information here
 */
typedef struct freq {
	char *srifproc;		/* Name of external (SRIF) FREQ processor   */
	s_frlist *frlist;	/* File areas list for int. FREQ processor  */
	s_reqlist *reqlist;	/* List of requested file masks, etc..      */
	s_filelist **filelist;	/* Put here files we found and want to send */
	s_filelist **flast;     /* Pointer to the last entry in filelist    */
	int fnumber;            /* Total number of files, we found          */
	int fileslimit;		/* Maxmum number of files to send as answer */
	size_t fsize;           /* Total size of files, we found            */
	size_t sizelimit;	/* Maxmum size of files to send as answer   */
} s_freq;

/* req_bark.c */

/* req_proc.c */
void req_proc(char *reqname, s_filelist **filelist);

/* req_srif.c */
int req_createsrif(char *sname, char *req, char *rsp);
void req_addfilelist(char *listname, s_freq *freq);

/* req_wazo.c */
int req_readwazooreq(char *reqname, s_reqlist **reqlist);

#endif
