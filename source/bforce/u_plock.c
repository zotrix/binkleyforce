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
#include "outbound.h"

pid_t plock_read(const char *lockname)
{
	FILE *fp = NULL;
	pid_t pid = 0;
	
	ASSERT(lockname != NULL);

	if( (fp = file_open(lockname, "rt")) )
	{
		if( fscanf(fp, "%d", &pid) != 1 )
			pid = 0;
		file_close(fp);
	}
	
	DEB((D_OUTBOUND, "plock_read: file \"%s\", PID = %d", lockname, pid));
	
	return pid;
}

pid_t plock_write(const char *lockname)
{
	int fd, rc = PLOCK_OK;
	char buf[32];
	
	ASSERT(lockname != NULL);
	
	if( (fd = open(lockname, O_CREAT | O_RDWR, 0644)) < 0 )
		/* Don't log any errors, really we can have
		 * no outbound directory for this address */
		return PLOCK_ERROR;

	chmod(lockname, 0644);

	sprintf(buf, "%10d\n", (int)getpid());
	
	if( write(fd, (char *)buf, strlen(buf)) != strlen(buf) )
	{
		logerr("can't write PID to the lock file \"%s\"", lockname);
		rc = PLOCK_ERROR;
	}

	close(fd);

	if( rc != PLOCK_OK )
		unlink(lockname);

	return rc;
}

/*
 * Return codes:
 *   PLOCK_OK
 *   PLOCK_EXIST
 *   PLOCK_ERROR
 *   PLOCK_OURBSY
 */
int plock_check(const char *lockname)
{
	pid_t pid = 0;
	struct stat st;
	
	ASSERT(lockname != NULL);
	
	if( stat(lockname, &st) && errno == ENOENT )
		return PLOCK_OK;
	
	if( (pid = plock_read(lockname)) == 0 )
		return (errno == ENOENT) ? PLOCK_OK : PLOCK_ERROR;

	if( pid == getpid() )
		return PLOCK_OURLOCK; /* Should we unlink it? */
	
	if( kill(pid, 0) == -1 && errno == ESRCH )
	{
		/*
		 * That process no longer exists,
		 * try to remove lock file
		 */
		log("remove stale lock file \"%s\", PID %d",
			lockname, (int)pid);
		
		if( !unlink(lockname) )
			return PLOCK_OK;
		else
		{
			logerr("cannot remove lock file");
			return PLOCK_ERROR;
		}
	}

	return PLOCK_EXIST; /* locking process still lives */
}

int plock_link(const char *lockname, const char *tmpname)
{
	int rc;
	
	ASSERT(lockname != NULL && tmpname != NULL);
	
	if( !link(tmpname, lockname) )
	{
		unlink(tmpname);
		return PLOCK_OK;
	}

	/* Can't create link */
	if( errno == EEXIST )
	{
		if( (rc = plock_check(lockname)) != PLOCK_OK )
		{
			if( rc == PLOCK_OURLOCK )
				rc = PLOCK_OK;
		}
		else if( link(tmpname, lockname) )
		{
			logerr("cannot link lock \"%s\" to \"%s\"", tmpname, lockname);
			rc = PLOCK_ERROR;
		} else
			rc = PLOCK_OK;
	}
	else
	{
		logerr("cannot link lock \"%s\" to \"%s\"", tmpname, lockname);
		rc = PLOCK_ERROR;
	}

	if( unlink(tmpname) == -1 && errno != ENOENT )
		logerr("cannot remove lock file \"%s\"", tmpname);
	
	return rc;
}

int plock_create(const char *lockname)
{
	int rc;
	char *tmpname, *p;
	
	ASSERT(lockname != NULL);
	
	tmpname = xstrcpy(lockname);
	if( (p = strrchr(tmpname, DIRSEPCHR)) && p != tmpname )
		*++p = '\0';
	else
	{
		free(tmpname);
		return PLOCK_ERROR;
	}
	tmpname = xstrcat(tmpname, "bforce-XXXXXX");

	if( (p = mktemp(tmpname)) == NULL )
	{
		logerr("can't generate unique file name from \"%s\"", tmpname);
		free(tmpname);
		return PLOCK_ERROR;
	}
	
	if( (rc = plock_write(p)) == PLOCK_OK )
		rc = plock_link(lockname, p);
	
	DEB((D_OUTBOUND, "out_bsy_createfile: createlink(\"%s\", \"%s\"), rc = %d",
		lockname, p, rc));
	
	free(tmpname);
	
	return rc;
}

int plock_remove(const char *lockname)
{
	pid_t pid = 0;
	
	ASSERT(lockname != NULL);
	
	DEB((D_OUTBOUND, "out_bsy_unlinkfile: want unlink \"%s\"", lockname));
	
	if( (pid = plock_read(lockname)) == 0 )
		return 1;
	
	if( pid == getpid() && unlink(lockname) == -1 )
		return 1;
	
	return 0;
}

