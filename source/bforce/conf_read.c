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

#define COMMENTCHR	'#'
#define PREPROCCHR	'$'
#define ESCAPECHR	'\\'

int conf_postreadcheck(void)
{
	return 0;
}

const char *conf_getconfname(void)
{
	const char *name = getenv("BFCONFIG");
	if( name == NULL || *name == '\0' ) name = BFORCE_CFGFILE;
	return(name);
}

int conf_readpasswdlist(s_falist **pwdlist, char *fname)
{
	return(0);
}

/*
 *  Prepare config string for parsing, check for comments
 *  Return: zero - good string, non-zero - .. (you know) 
 */
static int conf_checkstr(char *str)
{
	int rc = 0;
	char *p = NULL;
	
	ASSERT(str != NULL);
	
	if( isspace(*str) ) string_trimleft(str);
	if( *str == '\0' || *str == COMMENTCHR )
		{ rc = 1; }

	if( (p = strrchr(str, COMMENTCHR)) && *(p-1) != ESCAPECHR )
		{ *p = '\0'; }
	
	string_trimright(str);
	
	return rc;
}

/*
 *  Parse config string on: keyword, expression and value
 *  Return: zero - no errors, non-zero - incorrect syntax
 */
static void conf_parsestr(char *str, char **key, char **expr, char **value)
{
	char *k = NULL;		/* Keyword */
	char *e = NULL;		/* Expression */
	char *v = NULL;		/* Value */
	char *p = NULL;
	int brackets = 0;

	ASSERT(str != NULL && key != NULL && expr != NULL && value != NULL);
	
	*key = NULL; *expr = NULL; *value = NULL;
	
	/* Get config keyword */
	k = str;
	while( isspace(*k) ) k++;
	p = k;
	while( !isspace(*p) && *p ) p++;
	if( *p == '\0' )
		{ *key = k; return; }
	*p++ = '\0';
	
	while( isspace(*p) ) p++;
	v = p;
	
	/* Get config expression (if specified) */
	if( *p == '(' )
	{
		++p;
		while( isspace(*p) ) p++;
		for( e = p, brackets = 1; brackets > 0 && *p; )
		{
			if( *(p-1) != ESCAPECHR )
			{
				switch( *p ) {
				case '(':
					++brackets; break;
				case ')':
					--brackets; break;
				}
			}
			if( brackets > 0 ) ++p;
		}
		if( *p != ')' ) return;	/* No closing bracket */
		/* Remove trailing spaces */
		v = p--;
		while( p > e && isspace(*p) ) *p-- = '\0';
		*v++ = '\0';
	}
	
	/* Get config value */
	while( isspace(*v) ) v++;
	if( *v == '\0' )
		{ *key = k; *expr = e; return; }
	/* Remove trailing spaces */
	p = v + strlen(v+1);
	while( p > v && isspace(*p) ) *p-- = '\0';
	
	/* Return information to function caller */
	*key   = k;
	*expr  = e;
	*value = v;

	return;
}

int conf_readconf(const char *confname, int inclevel)
{
	FILE *fp = NULL;
	char tmp[BF_MAXCFGLINE + 1];
	int rc, maxrc = 0;
	int line      = 0;
	char *fullstr = NULL; /* Result of strings concatenation */
	char *value   = NULL;
	char *expr    = NULL;
	char *p_key   = NULL;
	char *p_expr  = NULL;
	char *p_value = NULL;
	char *p       = NULL;
	char *ifexpr  = NULL;  /* Pointer to the `common' block expression */
	bool isappend = FALSE; /* Append new string to the previous? */
	bool isifexpr = FALSE; /* We are in `common' config block! */
    
	if( inclevel == 0 )
	{
		int i;
		
		/* Check config table correctness */
		for( i = 0; bforce_config[i].name; i++ )
		{
			if( i != (int)bforce_config[i].real_key )
			{
				log("invalid config table: found %d instead of %d",
					bforce_config[i].real_key, i);
				return -1;
			}
		}
	}
	
	if( (fp = file_open(confname,"r")) == NULL )
	{
		logerr("can't open config file \"%s\"", confname);
		return(PROC_RC_IGNORE);
	}

	DEB((D_CONFIG, "readconfig: start reading config \"%s\"", confname));

	while( fgets(tmp, sizeof(tmp), fp) )
	{
		++line;
		string_chomp(tmp);
		
		if( conf_checkstr(tmp) )
		{
			if( isifexpr )
			{
				log("warning: automatically close expression at empty line %d", line);
				if( ifexpr ) { free(ifexpr); ifexpr = NULL; }
				isifexpr = FALSE;
			}
			
			if( !isappend )
				continue;
			
			log("warning: appending empty or comment line %d", line);
			isappend = FALSE;
		}
		
		p = tmp + strlen(tmp+1);
		
		if( *p == '\\' && *(p-1) != ESCAPECHR )
		{
			*p = '\0';
			if( fullstr && isappend )
			{
				fullstr = xstrcat(fullstr, tmp);
			}
			else
			{
				if( fullstr ) free(fullstr);
				fullstr = xstrcpy(tmp);
				isappend = TRUE;
			}
			continue;
		}
		else if( isappend )
		{
			if( fullstr )
				{ fullstr = xstrcat(fullstr, tmp); }
			else
				{ ASSERT_MSG(); }
			isappend = FALSE;
		}
		
		/*
		 *  Now we have string without comments
		 *  and trailing/leading spaces in:
		 *  - if single string than in $tmp[]
		 *  - if concatanated strings than look $fullstr 
		 */
		
		p_key   = NULL;
		p_expr  = NULL;
		p_value = NULL;

		conf_parsestr(fullstr ? fullstr : tmp, &p_key, &p_expr, &p_value);
		
		DEB((D_CONFIG, "conf_readconf: [%d] key \"%s\" expr \"%s\" value \"%s\"",
			line, p_key, p_expr, p_value));
		
		if( p_value )
		{
			value = xstrcpy(p_value);
			string_dequote(value, p_value);
		}

		if( p_expr )
		{
			expr = xstrcpy(p_expr);
			string_dequote(expr, p_expr);
		}
		
		if( p_key && *p_key == PREPROCCHR )
		{
			/* Preprocessor directives */
				
			rc = PROC_RC_OK; 
			
			if( strcasecmp(p_key+1, "include") == 0 )
			{
				if( value == NULL || expr )
				{
					log("incorrect usage of `%s' directive", p_key);
					rc = PROC_RC_IGNORE;
				}
				else if( inclevel < MAXINCLUDELEVEL )
				{
					DEB((D_CONFIG, "conf_readconf: process inlude file \"%s\"", value));
					rc = conf_readconf(value, inclevel + 1);
					if( rc ) rc = PROC_RC_IGNORE;
				}
				else
				{
					DEB((D_CONFIG, "conf_readconf: too deep include"));
					log("including level too deep");
					rc = PROC_RC_IGNORE;
				}
			}
			else if( strcasecmp(p_key+1, "ifexp") == 0 )
			{
				if( value || isifexpr )
				{
					log("incorrect usage of `%s' directive", p_key);
					rc = PROC_RC_ABORT;
				}
				else
				{
					if( ifexpr )
						{ free(ifexpr); ifexpr = NULL; }
					if( expr )
						ifexpr = xstrcpy(expr);
					isifexpr = TRUE;
				}
			}
			else if( strcasecmp(p_key+1, "endif") == 0 )
			{
				if( value || expr || !isifexpr )
				{
					log("incorrect usage of `%s' directive", p_key);
					rc = PROC_RC_IGNORE;
				}
				else
				{
					if( ifexpr ) { free(ifexpr); ifexpr = NULL; }
					isifexpr = FALSE;
				}
			}
			else if( strcasecmp(p_key+1, "logfile") == 0 )
			{
				if( value == NULL || expr )
				{
					log("incorrect usage of `%s' directive", p_key);
					rc = PROC_RC_IGNORE;
				}
				else
				{
					if( log_reopen(value, NULL, NULL) )
					{
						log("can't continue without logging");
						rc = PROC_RC_ABORT;
					}
				}
			}
			else if( strcasecmp(p_key+1, "debugfile") == 0 )
			{
				if( value == NULL || expr )
				{
					log("incorrect usage of `%s' directive", p_key);
					rc = PROC_RC_IGNORE;
				}
				else
#ifdef DEBUG
				{
					if( debug_isopened() ) debug_close();
					debug_setfilename(value);
				}
#else
				{
					log("incorrect directive `%s': no debug code", p_key);
					rc = PROC_RC_IGNORE;
				}
#endif
			}
			else if( strcasecmp(p_key+1, "debuglevel") == 0 )
			{
				if( value == NULL || expr )
				{
					rc = PROC_RC_IGNORE;
				}
				else
#ifdef DEBUG
				{
					unsigned long newlevel = 0L;
					
					if( debug_parsestring(value, &newlevel) )
					{
						rc = PROC_RC_WARN;
					}
					debug_setlevel(newlevel, 1);
				}
#else
				{
					log("incorrect directive `%s': no debug code", p_key);
					rc = PROC_RC_IGNORE;
				}
#endif
			}
			else
			{
				log("unknown directive `%s'", p_key);
				rc = PROC_RC_IGNORE;
			}
		}
		else if( p_key && value )
		{
			if( isifexpr && expr )
			{
				log("can't use expressions inside $ifexpr block");
				rc = PROC_RC_IGNORE;
			}
			else if( isifexpr )
			{
				rc = proc_configline(p_key, ifexpr, value);
			}
			else
			{
				rc = proc_configline(p_key, expr, value);
			}
		}
		else
		{
			log("incorrect config string");
			rc = PROC_RC_IGNORE;
		}
		
		if( fullstr )
			{ free(fullstr); fullstr = NULL; }
		if( value )
			{ free(value); value = NULL; }
		if( expr )
			{ free(expr); expr = NULL; }
		
		switch(rc) {
		case PROC_RC_OK:
			break;
		case PROC_RC_WARN:
			log("warning in config \"%s\" line %d", confname, line);
			break;
		case PROC_RC_IGNORE:
			log("ignore line %d in config \"%s\"", line, confname);
			break;
		case PROC_RC_ABORT:
			log("fatal error in config \"%s\" in line %d", confname, line);
			break;
		default:
			ASSERT_MSG();
			break;
		}
		if( rc == PROC_RC_ABORT ) { maxrc = 1; break; }
	} /* end of while */
	
	file_close(fp);

	if( isifexpr )
		{ maxrc = 1; log("unterminated directive `#ifexp'"); }
	
	if( fullstr )
		free(fullstr);
	if( ifexpr )
		free(ifexpr);
	
	DEB((D_CONFIG, "readconfig: exit with maxrc = %d", maxrc));

	return maxrc;
}

#ifdef DEBUG
void debug_override(void)
{
	s_cval_entry *ptrl;
	char abuf[BF_MAXADDRSTR+1];
	
	DEB((D_CONFIG, "debug_override: BEGIN"));
	
	for( ptrl = bforce_config[cf_override].data; ptrl; ptrl = ptrl->next )
	{
		DEB((D_CONFIG, "log_overridelist: address %s",
				ftn_addrstr(abuf, ptrl->d.override.addr)));
		DEB((D_CONFIG, "log_overridelist: \tIpaddr   = \"%s\"",
				ptrl->d.override.sIpaddr));
		DEB((D_CONFIG, "log_overridelist: \tPhone    = \"%s\"",
				ptrl->d.override.sPhone));
		DEB((D_CONFIG, "log_overridelist: \tWorktime = \"%s\"",
				timevec_string(abuf, &ptrl->d.override.worktime, sizeof(abuf))));
		DEB((D_CONFIG, "log_overridelist: \tFlags    = \"%s\"",
				ptrl->d.override.sFlags));
	}

	DEB((D_CONFIG, "debug_override: END"));
}
#endif /* DEBUG */

