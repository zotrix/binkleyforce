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

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "confread.h"

/* enable this to use syslog facility */

/* #define USE_SYSLOG */



enum { LOG_MODE_FILE, LOG_MODE_SYSLOG, LOG_MODE_MIXED };

enum { LOG_FILE_DAEMON, LOG_FILE_SESSION, LOG_FILE_DEBUG, LOG_FILE_HISTORY };

enum {
	LOG_EMERG   = 0, /* system is unusable */
	LOG_ALERT   = 1, /* action must be taken immediately */
	LOG_CRIT    = 2, /* critical conditions */
	LOG_ERR     = 3, /* error conditions */
	LOG_WARNING = 4, /* warning conditions */
	LOG_NOTICE  = 5, /* normal but significant condition */
	LOG_INFO    = 6, /* informational */
	LOG_DEBUG   = 7  /* debug-level messages */
};

#ifdef DEBUG
# define D_CONFIG	0x0000001L
# define D_OVERRIDE	0x0000002L
# define D_EVENT	0x0000004L
# define D_NODELIST	0x0000008L
# define D_OUTBOUND	0x0000010L
# define D_INFO		0x0000020L
# define D_HSHAKE	0x0000040L
# define D_TTYIO	0x0000080L
# define D_MODEM	0x0000100L
# define D_PROT		0x0000200L
# define D_FREQ		0x0000400L
# define D_STATEM	0x0000800L
# define D_DAEMON	0x0001000L
# define D_FULL		0xfffffffL
#endif

#ifdef DEBUG
# define DEB(what)	debug what
#else
# define DEB(what)
#endif

const char *log_getfilename(int whatfor);
bool log_isopened(void);
/* #ifndef USE_SYSLOG
void log_setident(const char *ident);
#endif */ /* ifndef USE_SYSLOG */
int  log_open(const char *logname, const char *ext, const char *tty);
int  log_close(void);
int  log_reopen(const char *logname, const char *ext, const char *tty);
int  log(const char *s, ...);
int  logerr(const char *s, ...);

#ifdef DEBUG
void debug_setlevel(long newlevel, bool logit);
bool debug_isopened(void);
void debug_setfilename(const char *debugname);
int  debug_parsestring(char *str, unsigned long *deblevel);
int  debug_open(const char *debugname);
int  debug_close(void);
int  debug(unsigned long what, const char *str, ...);
#endif

#endif

