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
#include "freq.h"

int req_readwazooreq(char *reqname, s_reqlist **reqlist)
{
	s_reqlist **tmpl;
	char s[BF_MAXPATH+1];
	char *p, *fname, *pwd = NULL, *older = NULL, *newer = NULL;
	char *p_ignorelist = NULL;
	FILE *fp;
	
	DEB((D_FREQ, "req_readreq: open '.req' file = \"%s\"", reqname));
	
	if( (fp = file_open(reqname, "r")) == NULL )
	{
		logerr("can't open req file \"%s\"", reqname);
		return(1);
	}

	/* $tmpl must point to last entry's next field */
	for( tmpl = reqlist; *tmpl; tmpl = &(*tmpl)->next )
	{
		/* EMPTY LOOP */
	}

	/* Get file masks that we should ignore */
	p_ignorelist = conf_string(cf_freq_ignore_masks);
	
	while( fgets(s, sizeof(s), fp) )
	{
		p     = s;
		fname = NULL;
		pwd   = NULL;
		older = NULL;
		newer = NULL;
		
		string_chomp(s);
		
		/* Remove leading spaces */
		while( isspace(*p) ) ++p;
		if( *p == '\0' ) continue;	/* Empty line :( */
		
		/* Get file name */
		fname = p;
		while( *p && !isspace(*p) ) ++p;

		if( *p ) *p++ = '\0';

		/* Check our ignore list */
		if( p_ignorelist && *p_ignorelist && fname && *fname )
		{
			if( !checkmasks(p_ignorelist, fname) )
			{
				log("FREQ: ignore request \"%s\"", fname);
				continue;
			}
		}
				
		/* Is there possible password or update time? */
		while( *p )
		{
			/* Remove spaces between fields */
			while( isspace(*p) ) ++p;
			
			switch( *p ) {
			case '!' : pwd   = ++p; break;
			case '+' : older = ++p; break;
			case '-' : newer = ++p; break;
			}
			/* Get field */
			while( *p && !isspace(*p) ) ++p;
			if( *p ) *p++ = '\0';
		}
		
		if( fname && *fname )
		{
			DEB((D_FREQ, "req_readreq: file=\"%s\", pwd=\"%s\", newer=\"%s\", older=\"%s\"", fname, pwd, newer, older));
			
			*(tmpl) = (s_reqlist*)xmalloc(sizeof(s_reqlist));
			memset(*tmpl, '\0', sizeof(s_reqlist));
			
			(*tmpl)->fmask = (char*)xstrcpy(fname);
			if( pwd   && *pwd   ) (*tmpl)->passwd = (char*)xstrcpy(pwd);
			if( newer && *newer ) (*tmpl)->newer = atol(newer);
			if( older && *older ) (*tmpl)->older = atol(older);
			
			/* prepare $tmpl for next entry */
			tmpl = &(*tmpl)->next;
		}
	}

	DEB((D_FREQ, "req_readreq: close '.req' file"));
	
	file_close(fp);
	
	return(0);
}
