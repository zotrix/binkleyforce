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
#include "nodelist.h"
#define DEF_DOMAIN "fidonet.org"
/*
 *  Our fake expressions checker. Allways return FALSE (?)
 */
bool eventexpr(s_expr *expr)
{
	return FALSE;
}

static void usage(void)
{
	printf_usage("nodelist lookup utility",
		"usage: nlookup [-rmh] <address>\n"
		"\n"
		"options:\n"
		"  -r                show nodelist string\n"
		"  -m                show email address\n"
		"  -h                show this help message\n"
		"\n"
	);
}

void print_nodemail(const s_node *node)
{
	char abuf[BF_MAXADDRSTR+1];

	if( node->sysop && *node->sysop && strcmp(node->sysop, "<none>") )
	{
		char username[BNI_MAXSYSOP+1];
		
		strnxcpy(username, node->sysop, sizeof(node->sysop));
		string_replchar(username, ' ', '_');

		printf("%s@%s.fidonet.org\n", username,
			    ftn_addrstr_inet(abuf, node->addr));
	}
	
	fflush(stdout);
}

void print_nodeinfo(const s_node *node)
{
	char abuf[BF_MAXADDRSTR+1];
					
	printf("Address   : %s\n", ftn_addrstr(abuf, node->addr));
	printf("System    : %s\n", node->name);
	printf("Phone     : %s\n", node->phone);
	printf("Sysop     : %s\n", node->sysop);
	printf("Location  : %s\n", node->location);
	printf("Speed     : %lu\n", node->speed);
	printf("Flags     : %s\n", node->flags);

	if( node->worktime.num )
	{
		char timebuf[80];
		time_t unixtime = time(NULL);
		struct tm *now  = localtime(&unixtime);

		timevec_string(timebuf, &node->worktime, sizeof(timebuf));
		
		printf("Work time : %s (%s)\n", timebuf,
			timevec_check(&node->worktime, now) ? "false" : "true");
	}
	
	if( node->sysop && *node->sysop && strcmp(node->sysop, "<none>") )
	{
		char username[BNI_MAXSYSOP+1];
	
		strnxcpy(username, node->sysop, sizeof(node->sysop));
		string_replchar(username, ' ', '_');
		
		printf("e-mail    : %s@%s\n", username,
			ftn_addrstr_inet(abuf, node->addr));
	}

	fflush(stdout);
}

int main(int argc, char *argv[])
{
	s_node node;
	s_faddr addr;
	char ch;
	bool rawstring = FALSE;
	bool emailaddr = FALSE;
	
	/* Initialise random number generation */
	(void)srand((unsigned)time(0));
	/* Initialise current locale */
	(void)setlocale(LC_ALL, "");
	
	while( (ch=getopt(argc, argv, "hrm")) != EOF )
	{
		switch( ch ) {
		case 'h':
			usage();
			exit(BFERR_NOERROR);
		case 'r':
			rawstring = TRUE;
			break;
		case 'm':
			emailaddr = TRUE;
			break;
		default:
			usage();
			exit(BFERR_FATALERROR);
		}
	}

	if( optind >= argc || ftn_addrparse(&addr, argv[optind], FALSE) )
	{
		usage();
		exit(BFERR_FATALERROR);
	}

	if( conf_readconf(conf_getconfname(), 0) )
		exit(BFERR_FATALERROR);
	
	if( rawstring )
	{
		char buf[512];

		if( nodelist_lookup_string(buf, sizeof(buf), addr) == 0 )
			printf("%s\n", buf);
	}
	else if( nodelist_lookup(&node, addr) == 0 )
	{
	    if( emailaddr )
		print_nodemail(&node);
	    else
		print_nodeinfo(&node);
	}
	
	deinit_conf();

	exit(0);
}
