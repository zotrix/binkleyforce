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

enum { LINE_TYPE_MODEM, LINE_TYPE_TCPIP };

typedef struct {
	char *name;
	time_t holdtimer;
	int type;
} s_line;

static s_line  *lines_tab = NULL;
static int      lines_num = 0;

int daemon_line_add(const char *name, int type)
{
	int i;
	
	for( i = 0; i < lines_num; i++ )
		if( !strcmp(lines_tab[i].name, name) )
			return i;
	
	log("register new line \"%s\"", name);
	
	if( lines_tab )
	{
		lines_tab = (s_line *)xrealloc(lines_tab, sizeof(s_line)*(lines_num+1));
		memset(&lines_tab[lines_num], '\0', sizeof(s_line));
	}
	else
	{
		lines_tab = (s_line *)xmalloc(sizeof(s_line));
		memset(lines_tab, '\0', sizeof(s_line));
	}
	
	lines_tab[lines_num].name = xstrcpy(name);
	lines_tab[lines_num].type = type;

	return lines_num++;
}

void daemon_line_hold(const char *name, int holdtime)
{
	int i;
	
	for( i = 0; i < lines_num; i++ )
		if( !strcmp(lines_tab[i].name, name) )
		{
			timer_set(&lines_tab[i].holdtimer, holdtime);
			return;
		}
	
	i = daemon_line_add(name, 0);
	timer_set(&lines_tab[i].holdtimer, holdtime);
}

bool daemon_line_isready(const char *name)
{
	int i;
	
	for( i = 0; i < lines_num; i++ )
		if( !strcmp(lines_tab[i].name, name) )
		{
			if( !timer_running(lines_tab[i].holdtimer)
			 || timer_expired(lines_tab[i].holdtimer) )
				return TRUE;
			
			return FALSE;
		}

	(void)daemon_line_add(name, 0);
	return TRUE;
}

void daemon_lines_deinit(void)
{
	int i;
	
	for( i = 0; i < lines_num; i++ )
		if( lines_tab[i].name )
			free(lines_tab[i].name);
	
	if( lines_tab )
	{
		free(lines_tab);
		lines_tab = NULL;
	}
	
	lines_num = 0;
}

