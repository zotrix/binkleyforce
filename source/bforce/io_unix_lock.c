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
#include "io.h"

/*
 * Set lock files type: Ascii, binary or SVR4? (not implemented yet)
 */
#if (BFORCE_LOCK_TYPE == 1)
#  define LOCK_ASCII
#elif (BFORCE_LOCK_TYPE == 2)
#  define LOCK_BIN
#else
#  error "Not implemented lock files type (BFORCE_LOCK_TYPE)"
#endif

/*
 * Set lock files directory
 */
#ifndef BFORCE_LOCK_DIR
#  error "Not defined lock files directory (BFORCE_LOCK_DIR)"
#endif

/*
 * Set prefix for lock file names
 */
#ifndef BFORCE_LOCK_PREFIX
#  define BFORCE_LOCK_PREFIX "LCK.."
#endif

/*
 * How much times we will try to lock (if device
 * is allready locked by another process)
 */
#ifndef BFORCE_LOCK_TRIES
#  define BFORCE_LOCK_TRIES	2
#endif

/*
 * Delay in seconds between tries
 */
#ifndef BFORCE_LOCK_DELAY
#  define BFORCE_LOCK_DELAY	1
#endif

/* ========================================================================= */
/* "Low level" interface for tty lock files access (private)                 */
/*                                                                           */
/*   char *lock_getname(const char *lockdir, const s_modemport *modemport)   */
/*   pid_t lock_read_pid(const char *lckname)                                */
/*   int   lock_check(const char *lckname)                                   */
/*   int   lock_create(const char *lckname, const char *tmpname)             */
/*                                                                           */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* Get lock file name with full path  (this value must be free'ed)           */
/* ------------------------------------------------------------------------- */
static char *lock_getname(const char *lockdir, const s_modemport *modemport)
{
	char *lckname;
	char *p = NULL;
	
	ASSERT(modemport != NULL);
	
	if( lockdir && *lockdir )
		lckname = (char *)xstrcpy(lockdir);
	else
		lckname = (char *)xstrcpy(BFORCE_LOCK_DIR);
		
	lckname = (char *)xstrcat(lckname, BFORCE_LOCK_PREFIX);
	lckname = (char *)xstrcat(lckname, (p = port_get_name(modemport->name)));

	if( p )
		free(p);

	return lckname;
}

/* ------------------------------------------------------------------------- */
/* Get lock file's PID. Return zero on error and leave errno with last error */
/* ------------------------------------------------------------------------- */
static pid_t lock_read_pid(const char *lckname)
{
	int len, fd;
	pid_t pid;
	char buf[32];
	
	ASSERT(lckname != NULL);

	if( (fd = open(lckname, O_RDONLY)) < 0 )
		return 0;

	if( (len = read(fd, buf, sizeof(buf)-1)) < 0 )
	{
		close(fd);
		return 0;
	}
	
	buf[len] = '\0';
	
	if( len == sizeof(pid) || sscanf(buf, "%d", &pid) != 1 || pid == 0 )
	{
		/* We found binary lock file? */
		pid = *((int *)buf);
#ifndef LOCK_BINARY
		log("warning: found binary lock file %s", lckname);
#endif
	}
#ifdef LOCK_BINARY
	else
	{
		/* Found ASCII lock file */
		log("warning: found ascii lock file %s", lckname);
	}
#endif
	close(fd);
	
	return pid;
}

/* ------------------------------------------------------------------------- */
/* Validate lock file, check it's process still lives.                       */
/* Return:                                                                   */
/*   CHECKLOCK_NOLOCK  - no lock file found                                  */
/*   CHECKLOCK_LOCKED  - valid lock file                                     */
/*   CHECKLOCK_ERROR   - invalid lock file name, access error, etc.          */
/*   CHECKLOCK_OURLOCK - if it is our lock file (with our PID)               */
/* ------------------------------------------------------------------------- */
static int lock_check(const char *lckname)
{
	pid_t pid = 0;
	struct stat st;
	
	ASSERT(lckname != NULL);
	
	if( stat(lckname, &st) && errno == ENOENT )
		return LOCKCHECK_NOLOCK;

	if( (pid = lock_read_pid(lckname)) == 0 )
	{
		if( errno == ENOENT )
			return LOCKCHECK_NOLOCK;
		return LOCKCHECK_LOCKED;
	}

	DEB((D_MODEM, "lock_check: lock PID = %d", pid));

	if( pid == getpid() )
		{ return LOCKCHECK_OURLOCK; }
	
	if( kill(pid, 0) == -1 )
	{
		if( errno == ESRCH )
		{
			/*
			 * That process no longer exists, remove lock file
			 */
			log("found stale lock file with PID %d", pid);
			
			if( unlink(lckname) == -1 )
			{
				logerr("cannot remove lockfile \"%s\", lckname");
				return LOCKCHECK_ERROR;
			} else
				return LOCKCHECK_NOLOCK;
		}
		else
		{
			return LOCKCHECK_ERROR;
		}
	}

	/* locking process still lives */
	return LOCKCHECK_LOCKED;
}

/* ------------------------------------------------------------------------- */
/* Create lock file using link(tmpname, lckname). Return zero on success.    */
/* ------------------------------------------------------------------------- */
static int lock_create(const char *lckname, const char *tmpname)
{
	int rc, fd;
	int tries;
	
#ifdef LOCK_BINARY
	pid_t pid;
#else
	char buf[32];
#endif

	ASSERT(lckname != NULL && tmpname != NULL);

	if( (fd = open(tmpname, O_CREAT | O_RDWR, 0644)) < 0 )
	{
		logerr("can't open temp file \"%s\"", tmpname);
		return 1;
	}

	chmod(tmpname, 0644);

#ifdef LOCK_BINARY
	pid = getpid();
	rc = ( write(fd, (char *)&pid, sizeof(pid)) != sizeof(pid) );
#else
	sprintf(buf, "%10d\n", (int)getpid());
	rc = ( write(fd, (char *)buf, strlen(buf)) != strlen(buf) );
#endif
	close(fd);
	
	if( rc )
	{
		logerr("can't write PID to temp. lock file \"%s\"", tmpname);
		unlink(tmpname);
		return 1;
	}
	
	for( tries = 0; tries < BFORCE_LOCK_TRIES; tries++ )
	{
		if( link(tmpname, lckname) == -1 )
		{
			if( errno == EEXIST )
			{
				rc = lock_check(lckname);
				if( rc == LOCKCHECK_ERROR )
				{
					break;
				}
				else if( rc == LOCKCHECK_OURLOCK )
				{
					/* There is our lock! :) */
					unlink(tmpname);
					return 0;
				}
				else if( rc == LOCKCHECK_LOCKED )
				{
					/* sleep some time */
					sleep(BFORCE_LOCK_DELAY);
				}
			}
			else
			{
				log("can't create link to temp. lock file \"%s\"", lckname);
				break;
			}
		}
		else
		{
			/* Successful lock */
			unlink(tmpname);
			return 0;
		}
	}

	/* Can't create lock file */
	unlink(tmpname);
	return 1;
}

/* ========================================================================= */
/* PUBLIC functions for tty lock files access                                */
/*                                                                           */
/*   int port_checklock(const char *lockdir, const char *dev)                */
/*   int port_lock(char *lockdir, char *dev)                                 */
/*   int port_unlock(const char *lockdir, const char *dev)                   */
/*                                                                           */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* Check, is tty locked? Return one of return values of lock_check()         */
/* ------------------------------------------------------------------------- */
int port_checklock(const char *lockdir, const s_modemport *modemport)
{
	int rc;
	char *lckname;
	
	if( (lckname = lock_getname(lockdir, modemport)) == NULL )
	{
		log("can't get lock file name");
		return LOCKCHECK_ERROR;
	}
	
	DEB((D_MODEM, "checkttylock: lock device = \"%s\", lockfile = \"%s\"",
		modemport->name, lckname));
	
	rc = lock_check(lckname);
	
	if( lckname )
		free(lckname);
	
	return rc;
}

/* ------------------------------------------------------------------------- */
/* Lock serial port. On success return zero value                            */
/* ------------------------------------------------------------------------- */
int port_lock(const char *lockdir, const s_modemport *modemport)
{
	int rc;
	char *lckname;
	char *tmpname, *p_tmpname;
	
	if( lockdir && *lockdir )
		tmpname = xstrcpy(lockdir);
	else
		tmpname = xstrcpy(BFORCE_LOCK_DIR);
		
	tmpname = xstrcat(tmpname, "bfXXXXXX");
	
	if( (p_tmpname = mktemp(tmpname)) == NULL )
	{
		logerr("can't generate unique file name from \"%s\"", tmpname);
		free(tmpname); return 1;
	}

	if( (lckname = lock_getname(lockdir, modemport)) == NULL )
	{
		log("can't get lock file name");
		return(1);
	}
	
	DEB((D_MODEM, "locktty: lock device = \"%s\", tmpfile = \"%s\", lockfile = \"%s\"",
		modemport->name, p_tmpname, lckname));
	
	rc = lock_create(lckname, p_tmpname);
	
	if( tmpname )
		free(tmpname);
	if( lckname )
		free(lckname);
	
	return rc;
}

/* ------------------------------------------------------------------------- */
/* Unlock serial port. Return zero on success.                               */
/* ------------------------------------------------------------------------- */
int port_unlock(const char *lockdir, const s_modemport *modemport)
{
	pid_t pid = 0;
	char *lckname;
	
	if( (lckname = lock_getname(lockdir, modemport)) == NULL )
		return 1;
	
	DEB((D_MODEM, "unlocktty: unlock device = \"%s\", lockfile = \"%s\"",
			modemport->name, lckname));
	
	if( (pid = lock_read_pid(lckname)) == 0 )
	{
		free(lckname);
		return 1;
	}
	
	if( pid == getpid() && unlink(lckname) == -1 )
	{
		free(lckname);
		return 1;
	}
	
	free(lckname);
	
	return 0;
}

