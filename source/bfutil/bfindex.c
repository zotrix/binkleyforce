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
#include "version.h"
#include "logger.h"
#include "util.h"
#include "nodelist.h"

/*
 *  Our fake expressions checker. Allways return FALSE (?)
 */
bool eventexpr(s_expr *expr)
{
	return FALSE;
}

static void usage(void)
{
	printf_usage("nodelist compiler",
		"usage: bfindex [-fh]\n"
		"\n"
		"options:\n"
		"  -f                force nodelist compiling\n"
		"  -h                show this help message\n"
		"\n"
	);
}

static int nodelist_makeindex(s_nodelist *nlp, s_faddr addr)
{
	s_bni bni;
	char buf[1024];
	long countnodes = 0L;
	long countlines = 0L;
	enum nodelist_keyword keyword;

	memset(&bni, 0, sizeof(s_bni));
	bni.zone   = addr.zone;
	bni.net    = addr.net;
	bni.node   = addr.node;
	bni.point  = addr.point;
	
	if( nodelist_createheader(nlp) == -1 )
	{
		log("cannot create nodelist index header");
		return -1;
	}
	
	while(1)
	{
		char *p, *q;
		
		if( (bni.offset = ftell(nlp->fp_nodelist)) == -1 )
		{
			logerr("ftell() return -1 for nodelist");
			return -1;
		}
		
		if( fgets(buf, sizeof(buf), nlp->fp_nodelist) == NULL )
		{
			return countnodes;
		}
		
		string_chomp(buf);
		++countlines;
		
		if( buf[0] == ';' || buf[0] == '\0' ) continue;
		
		if( (p = strchr(buf, ',')) )
		{
			*p++ = '\0';
			if( (q = strchr(p, ',')) ) *q = '\0';
		}
		
		if( p == NULL || *p == '\0' )
		{
			log("incorrect nodelist line %ld: Short line", countlines);
			continue;
		}
		
		if( (keyword = nodelist_keywordval(buf)) == -1 )
		{
			log("incorrect nodelist line %d: Bad keyword \"%s\"",
				countlines, buf);
			continue;
		}
		
		if( keyword == KEYWORD_BOSS )
		{
			s_faddr tmpaddr;
			
			if( ftn_addrparse(&tmpaddr, p, FALSE) )
			{
				log("incorrect nodelist line %ld: Bad boss address \"%s\"",
					countlines, p);
			}
			else
			{
				bni.zone  = tmpaddr.zone;
				bni.net   = tmpaddr.net;
				bni.node  = tmpaddr.node;
				bni.point = 0;
				bni.hub   = 0;
			}
		}
		else if( ISDEC(p) )
		{
			int value = atoi(p);

			if( value > 0x7fff )
				value = 0x7fff;
			else if( value < 0 )
				value = 0;
		
			switch(keyword) {
			case KEYWORD_ZONE:
				bni.zone  = value;
				bni.net   = 0;
				bni.node  = 0;
				bni.point = 0;
				bni.hub   = 0;
				break;
			case KEYWORD_REGION:
				bni.net   = value;
				bni.node  = 0;
				bni.point = 0;
				bni.hub   = 0;
				break;
			case KEYWORD_HOST:
				bni.net   = value;
				bni.node  = 0;
				bni.point = 0;
				bni.hub   = 0;
				break;
			case KEYWORD_HUB:
				bni.node  = value;
				bni.point = 0;
				bni.hub   = value;
				break;
			case KEYWORD_EMPTY:
			case KEYWORD_PVT:
			case KEYWORD_HOLD:
			case KEYWORD_DOWN:
				bni.node  = value;
				bni.point = 0;
				break;
			case KEYWORD_POINT:
				bni.point = value;
				break;
			default:
				ASSERT_MSG();
			}
			
			if( nodelist_putindex(nlp, &bni) == -1 )
				return -1;
			
			++countnodes;
		}
		else
		{
			log("incorrect nodelist line %ld: Bad number \"%s\"",
				countlines, p);
		}
	}
	
	return -1;
}

int main(int argc, char *argv[])
{
	s_cval_entry *cfptr;
	char *nodelistdir = NULL;
	time_t starttime  = 0L;
	bool forcecompile = FALSE;
	long countnodes   = 0L;
	char c;
	
	/* Initialise random number generation */
	(void)srand((unsigned)time(0));
	/* Initialise current locale */
	(void)setlocale(LC_ALL, "");
	/* Set our name (for logging only) */
	
	while( (c = getopt(argc, argv, "hf")) != EOF )
	{
		switch( c ) {
		case 'f':
			forcecompile = TRUE;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	if( conf_readconf(conf_getconfname(), 0) )
	{
		exit(1);
	}

	if( log_open(log_getfilename(LOG_FILE_SESSION), NULL, NULL) )
	{
		log("can't continue without logging");
		exit(1);
	}
	
	/* Ignore some terminating signals */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	
	time(&starttime);
	
	nodelistdir = conf_string(cf_nodelist_directory);
	
	for( cfptr = conf_first(cf_nodelist); cfptr; cfptr = conf_next(cfptr) )
	{
		s_nodelist *nlp = NULL;
		bool willcompile = FALSE;
		
		if( forcecompile )
		{
			willcompile = TRUE;
		}
		else if( (nlp = nodelist_open(nodelistdir,
		                              cfptr->d.falist.what,
		                              NODELIST_READ)) == NULL )
		{
			willcompile = TRUE;
		}
		else
		{
			nodelist_close(nlp); nlp = NULL;
			willcompile = FALSE;
		}
		
		if( willcompile )
		{
			log("rebuilding index for nodelist \"%s\"", cfptr->d.falist.what);
			if( (nlp = nodelist_open(nodelistdir, cfptr->d.falist.what,
			                         NODELIST_WRITE)) )
			{
				long rc = nodelist_makeindex(nlp, cfptr->d.falist.addr);
				nodelist_close(nlp); nlp = NULL;
				if( rc > 0 ) countnodes += rc;
			}
		}
		else
			log("nodelist \"%s\" is up to date", cfptr->d.falist.what);
	}
	
	if( countnodes > 0 )
	{
		long ptime = (long)time(NULL) - (long)starttime;
		long nps = (ptime > 0) ? countnodes / ptime : countnodes;
		log("processed %ld nodes in %ld second(s) (%ld node/sec)",
			countnodes, ptime, nps);
	}

	deinit_conf();
	
	/*
	 * Shutdown logging services
	 */
	if( log_isopened() ) log_close();
#ifdef DEBUG
	if( debug_isopened() ) debug_close();
#endif
	
	exit(0);
}
