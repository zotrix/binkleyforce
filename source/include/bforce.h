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

#ifndef _BFORCE_H_
#define _BFORCE_H_

#ifndef DAEMON_LOGFILE
#define DAEMON_LOGFILE "/var/log/bforce/bf-daemon"
#endif

#ifndef BFORCE_LOGFILE
#define BFORCE_LOGFILE "/var/log/bforce/bf-log"
#endif

#ifndef BFORCE_DEBFILE
#define BFORCE_DEBFILE "/var/log/bforce/bf-debug"
#endif

#ifndef BFORCE_CFGFILE
#define BFORCE_CFGFILE "/etc/bforce/bforce.conf"
#endif

/*
 * BinkleyForce limits
 */
#define BF_MAXCFGLINE		512
#define BF_MAXREQLINE		256
#define BF_MAXPATH		256
#define BF_MAXDOMAIN		40
#define BF_MAXADDRSTR		80

/*
 * Maximum length of file name (without path)
 */
#define BFORCE_MAX_NAME         128

/*
 * Maximum path length, including file name length
 */
#define BFORCE_MAX_PATH         256

/*
 * Maximum length of address string
 */
#define BFORCE_MAX_ADDRSTR      80

/*
 * Lock files type:
 *   1 - Ascii lock files
 *   2 - Binary lock files
 *   3 - SVR4 lock files (not implemented)
 */
#define BFORCE_LOCK_TYPE        1

/*
 * Maximum length of phone number string
 */
#define BFORCE_MAX_PHONE        80

/*
 * Maximum length of modem command string. Most modems
 * cannot handle strings longer 40 characters.
 */
#define MODEM_MAX_COMMAND       120

/*
 * Maximum length of modem response string
 */
#define MODEM_MAX_RESP          120

/*
 * Maximum number of characters that modem can response
 * on something command. Responses larger this will be
 * considered invalid, and mailer will return "modem not
 * response" without further waiting for "OK" or "ERROR"
 */
#define MODEM_MAX_RESP_SIZE     8128

/*
 *  BinkleyForce return codes
 */
#define BFERR_NOERROR		0
#define BFERR_FATALERROR	1
#define BFERR_PHONE_UNKNOWN	2
#define BFERR_PORTBUSY		3
#define	BFERR_SYSTEM_LOCKED	4
#define	BFERR_TRY_LATER		5
#define	BFERR_NOMODEM		6
#define	BFERR_NOTWORKING	7
#define	BFERR_CANT_CONNECT10	10
#define	BFERR_CANT_CONNECT11	11
#define	BFERR_CANT_CONNECT12	12
#define	BFERR_CANT_CONNECT13	13
#define	BFERR_CANT_CONNECT14	14
#define	BFERR_CANT_CONNECT15	15
#define	BFERR_CANT_CONNECT16	16
#define	BFERR_CANT_CONNECT17	17
#define	BFERR_CANT_CONNECT18	18
#define	BFERR_CANT_CONNECT19	19
#define	BFERR_CONNECT_TOOLOW	20
#define	BFERR_HANDSHAKE_ERROR	21
#define	BFERR_XMITERROR		22
#define	BFERR_CPSTOOLOW		23
#define	BFERR_STOPTIME		24

#define BFERR_MAXERRNO		24	/* Maximal error number */

#define BFERR_NAME(rc)      ((rc >= 0) && (rc <= BFERR_MAXERRNO) ? BFERR[rc] : "Out of range")

/*
 *  Some most popular defines
 */
#define gotoexit(a)	{ rc = a; goto exit; }

#ifdef __GNUC__
#define ASSERT_MSG()	log("assertion failed: file: %s, line: %d, function: %s", \
				__FILE__, __LINE__, __FUNCTION__); 
#else
#define ASSERT_MSG()	log("assertion failed: file: %s, line: %d", \
				__FILE__, __LINE__);
#endif

#define ASSERT(expr) \
	if( !(expr) ) \
	{ \
		ASSERT_MSG(); \
		abort(); \
	}

#define ISHEX(s) (strspn(s, "0123456790abcdefABCDEF") == strlen(s))
#define ISDEC(s) (strspn(s, "0123456789"            ) == strlen(s))
#define ISOCT(s) (strspn(s, "01234567"              ) == strlen(s))

/*
 *  Some usable defines
 */
#define FALSE		0
#define TRUE		1
#define UNDEF		-1

/*
 *  Definition of new pretty types
 */
typedef char            bool;
typedef unsigned long   UINT32;
typedef unsigned short  UINT16;
typedef unsigned char   UINT8;
typedef signed long     SINT32;
typedef signed short    SINT16;
typedef signed char     SINT8;

#include "confread.h"

/*
 * Command line options
 */
typedef struct {
	bool   daemon;		/* Run as daemon?                 */
	bool   quit;		/* Quit from daemon               */
        bool   dontcall;        /* -m key */
	int    inetd;		/* Called from inetd?             */
	int    force;		/* Force call?                    */
	int    hiddline;	/* Hidden line number (0,1..)     */
	char  *confname;	/* Use this config instead def.   */
	char  *incname;		/* Include this config            */
	char  *phone;		/* Forced phone number            */
	char  *iaddr;		/* Forced IP address              */
	char  *connect;		/* Connect string                 */
	char  *device;		/* Forced device name             */
	int    stype;		/* Handshake type in slave mode   */
	s_falist *addrlist;
} s_bforce_opts;

/*
 *  Global variables
 */
extern const char *BFERR[];

int daemon_run(const char *confname, const char *incname, bool quit);

#endif
