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
#include "daemon.h"

static s_daemon_branch *daemon_branch_tab = NULL;
static signed int daemon_branch_num = 0;


#define ASSERT_POS(pos) ASSERT((pos) >= 0 && ((pos) + 1) <= daemon_branch_num \
                          && daemon_branch_tab)

bool daemon_branch_exist(s_faddr addr)
{
	int i;
	
	for( i = 0; i < daemon_branch_num; i++ )
		if( !ftn_addrcomp(daemon_branch_tab[i].addr, addr) )
			return TRUE;
	
	return FALSE;
}

void daemon_branch_check_stop_timers(void)
{
	int i;
	time_t now = time(0);

	for( i = 0; i < daemon_branch_num; i++ )
		if( daemon_branch_tab[i].stop && !daemon_branch_tab[i].wait
			&& now >= daemon_branch_tab[i].stop )
		{
			if( kill(daemon_branch_tab[i].pid, SIGKILL) == -1 )
				logerr("cannot kill branch with PID %d",
						(int)daemon_branch_tab[i].pid);
		}
}

int daemon_branch_exit(pid_t pid, int rc)
{
	int i;
	
	for( i = 0; i < daemon_branch_num; i++ )
		if( daemon_branch_tab[i].pid == pid )
		{
			daemon_branch_tab[i].rc = rc;
			daemon_branch_tab[i].wait = TRUE;
			return 0;
		}
	
	return -1;
	
}

int daemon_branch_get_first_waiting(void)
{
	int i;
	
	for( i = 0; i < daemon_branch_num; i++ )
		if( daemon_branch_tab[i].wait )
			return i;
	
	return -1;
}

s_daemon_branch *daemon_branch_get_pointer(int pos)
{
	ASSERT_POS(pos);
	
	if( pos + 1 > daemon_branch_num )
	{
		log("internal error: branch position is out of range");
		return NULL;
	}
	
	return &daemon_branch_tab[pos];
}

void daemon_branch_add(s_faddr addr, pid_t pid, bool tcpip, const char *pname)
{
	if( daemon_branch_tab )
	{
		daemon_branch_tab =
			(s_daemon_branch *)xrealloc(daemon_branch_tab,
			sizeof(s_daemon_branch)*(daemon_branch_num + 1));
		memset(&daemon_branch_tab[daemon_branch_num], '\0',
			sizeof(s_daemon_branch));
	}
	else
	{
		daemon_branch_tab =
			(s_daemon_branch *)xmalloc(sizeof(s_daemon_branch));
		memset(daemon_branch_tab, '\0', sizeof(s_daemon_branch));
		daemon_branch_num = 0;
	}
	
	daemon_branch_tab[daemon_branch_num].addr  = addr;
	daemon_branch_tab[daemon_branch_num].wait  = FALSE;
	daemon_branch_tab[daemon_branch_num].tcpip = tcpip;
	daemon_branch_tab[daemon_branch_num].pid   = pid;
	daemon_branch_tab[daemon_branch_num].start = time(NULL);
	
	if( pname )
		daemon_branch_tab[daemon_branch_num].portname = xstrcpy(pname);
	
	daemon_branch_num++;
}

void daemon_branch_remove(int pos)
{
	ASSERT_POS(pos);
	
	if( daemon_branch_tab[pos].portname )
		free(daemon_branch_tab[pos].portname);
	
	if( daemon_branch_num > 1 )
	{
		memmove(&daemon_branch_tab[pos],
			&daemon_branch_tab[pos+1],
			sizeof(s_daemon_branch)*(daemon_branch_num - pos - 1));
		daemon_branch_tab =
			(s_daemon_branch *)xrealloc(daemon_branch_tab,
			sizeof(s_daemon_branch)*(daemon_branch_num - 1));
		daemon_branch_num--;
	}
	else
	{
		free(daemon_branch_tab);
		daemon_branch_tab = NULL;
		daemon_branch_num = 0;
	}
}

int daemon_branch_number(void)
{
	return daemon_branch_num;
}

void daemon_branch_deinit(void)
{
	if( daemon_branch_tab )
		free(daemon_branch_tab);
	
	daemon_branch_tab = NULL;
	daemon_branch_num = 0;
}

