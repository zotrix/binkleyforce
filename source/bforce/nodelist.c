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

#define NODELIST_LOCK_TRIES 3
#define NODELIST_LOCK_DELAY 1 /* seconds */

struct keyword {
	const char *keystr;
	const int keyval;
} keywords[] = {
	{ "",       KEYWORD_EMPTY  },
	{ "Zone",   KEYWORD_ZONE   },
	{ "Region", KEYWORD_REGION },
	{ "Host",   KEYWORD_HOST   },
	{ "Hub",    KEYWORD_HUB    },
	{ "Pvt",    KEYWORD_PVT    },
	{ "Hold",   KEYWORD_HOLD   },
	{ "Down",   KEYWORD_DOWN   },
	{ "Boss",   KEYWORD_BOSS   },
	{ "Point",  KEYWORD_POINT  },
	{ NULL,     0              }
};

/*****************************************************************************
 * Check flag for existing in nodelist flags string
 *
 * Arguments:
 * 	nodeflags pointer to the node's nodelist flags string
 * 	flag      pointer to the flag that we want to check
 *
 * Return value:
 * 	zero value if flag is presented in flags, and non-zero if not
 */
int nodelist_checkflag(const char *nodeflags, const char *flag)
{
	char *p, *q;
	
	if( (p = strstr(nodeflags, flag)) )
	{
		if( p == nodeflags || *(p-1) == ',' )
		{
			if( (q = strchr(p, ',')) == NULL || (q - p) == strlen(flag) )
				return 0;
		}
	}
	
	return 1;
}

/*****************************************************************************
 * Get nodelist keyword (e.g. Host, Hub, Point, etc.) value
 * (e.g. KEYWORD_HOST, KEYWORD_HUB, etc.)
 *
 * Arguments:
 * 	keywordval pointer to the keyword string
 *
 * Return value:
 * 	One of KEYWORD_* values, and -1 for unknown keywords
 */
int nodelist_keywordval(const char *keyword)
{
	int i;
	
	for( i = 0; keywords[i].keystr; i++ )
	{
		if( strcmp(keyword, keywords[i].keystr) == 0 )
		{
			return keywords[i].keyval;
		}
	}
	return -1;
}

int nodelist_parse_Txy(s_node *node, const char *xy)
{
	long beg;
	long end;
	long tz;
	
	if( xy[0] >= 'A' && xy[0] <= 'X' )
		beg = (xy[0] - 'A') * 60;
	else if( xy[0] >= 'a' && xy[0] <= 'x' )
		beg = (xy[0] - 'a') * 60 + 30;
	else
		return -1;
	
	if( xy[1] >= 'A' && xy[1] <= 'X' )
		end = (xy[1] - 'A') * 60;
	else if( xy[1] >= 'a' && xy[1] <= 'x' )
		end = (xy[1] - 'a') * 60 + 30;
	else
		return -1;
	
	if( beg == end )
		return -1;
	
	/* Convert it to local time */
	if( (tz = time_gmtoffset()) )
	{
		beg -= tz;
		if( beg > 1440 )
			beg = beg - 1440;
		else if( beg < 0 )
			beg = beg + 1440;
		else if( beg == 1440 )
			beg = 0;
		
		end -= tz;
		if( end > 1440 )
			end = end - 1440;
		else if( end < 0 )
			end = end + 1440;
		else if( end == 1440 )
			end = 0;
	}
	
	timevec_add(&node->worktime, DAY_MONDAY, DAY_SUNDAY, beg, end);
	
	return 0;
}

/*****************************************************************************
 * Nodelist string parser
 *
 * Arguments:
 * 	node      put here all obtained information
 * 	str       pointer to the nodelist string
 *
 * Return value:
 * 	zero value if string was parsed successfuly, and non-zero if wasn't
 */
int nodelist_parsestring(s_node *node, char *str)
{
	char *argv[NODELIST_POSFLAGS+1];
	char *p;
	
	if( string_parse(argv, NODELIST_POSFLAGS+1, str, ',') != NODELIST_POSFLAGS+1 )
		return -1;
	
	if( !ISDEC(argv[NODELIST_POSNUMBER]) || !ISDEC(argv[NODELIST_POSSPEED]) )
		return -1;
	if( (node->keyword = nodelist_keywordval(argv[NODELIST_POSKEYWORD])) == -1 )
		return -1;
	
	if( node->addr.zone )
	{
		int  number = atoi(argv[NODELIST_POSNUMBER]);
		bool goodstr = FALSE;
		
		switch(node->keyword) {
		case KEYWORD_ZONE:
			goodstr = (node->addr.zone == number);
			break;
		case KEYWORD_REGION:
		case KEYWORD_HOST:
			goodstr = (node->addr.net == number);
			break;
		case KEYWORD_POINT:
			goodstr = (node->addr.point == number);
			break;
		default:
			goodstr = (node->addr.node == number);
			break;
		}
		if( !goodstr ) return -1;
	}
	
	strnxcpy(node->name, argv[NODELIST_POSNAME], sizeof(node->name));
	strnxcpy(node->location, argv[NODELIST_POSLOCATION], sizeof(node->location));
	strnxcpy(node->sysop, argv[NODELIST_POSSYSOP], sizeof(node->sysop));
	strnxcpy(node->phone, argv[NODELIST_POSPHONE], sizeof(node->phone));
	strnxcpy(node->flags, argv[NODELIST_POSFLAGS], sizeof(node->flags));
	node->speed = atoi(argv[NODELIST_POSSPEED]);
	
	/*
	 * Replace all '_' by space character
	 */
	string_replchar(node->name, '_', ' ');
	string_replchar(node->location, '_', ' ');
	string_replchar(node->sysop, '_', ' ');
	
	/*
	 * Get system work time (usefull flags: CM,Txy)
	 */
	if( nodelist_checkflag(node->flags, "CM") == 0 )
	{
		timevec_add(&node->worktime, DAY_MONDAY, DAY_SUNDAY, 0, 1440);
	}
	else
	{
		for( p = node->flags; p && *p; p = strchr(p, ',') )
		{
			if( p[1] == 'T' && p[2] && p[3] && (p[4] == ',' || p[4] == '\0') )
			{
				if( nodelist_parse_Txy(node, p+2) == -1 )
					log("invalid nodelist Txy flag in \"%s\"", node->flags);
				break;
			}
			p++;
		}
		
		/*
		 * Set default work time according to ZMH
		 */
		if( node->addr.point == 0 )
		{
			switch(node->addr.zone) {
			case 1: nodelist_parse_Txy(node, "JK"); break;
			case 2: nodelist_parse_Txy(node, "cd"); break;
			case 3: nodelist_parse_Txy(node, "ST"); break;
			case 4: nodelist_parse_Txy(node, "IJ"); break;
			case 5: nodelist_parse_Txy(node, "BC"); break;
			case 6: nodelist_parse_Txy(node, "UV"); break;
			}
		}
	}
	
	return 0;
}

/*****************************************************************************
 * Open nodelist, nodelist index, do some checks
 *
 * Arguments:
 * 	dir       pointer to the nodelist's directory
 * 	name      pointer to the nodelist file name
 * 	mode      are we going to read nodelist index? Or write?
 *
 * Return value:
 * 	pointer to the allocated nodelist structure (will be used with all
 * 	nodelist operations), and NULL to indicate errors.
 */
s_nodelist *nodelist_open(const char *dir, char *name, int mode)
{
	s_nodelist tmp;
	const char *openmode;
	int lockmode;
	char *ext; /* extension */
	memset(&tmp, '\0', sizeof(s_nodelist));
	char *lastname;
	/*
	 * Select nodelist index open mode
	 */
	if( mode == NODELIST_READ )
	{
		openmode = "r";
		lockmode = FILELOCK_READ;
	}
	else if( mode == NODELIST_WRITE )
	{
		openmode = "w";
		lockmode = FILELOCK_WRITE;
	}
	else
		ASSERT(0);
	
	/*
	 * Get nodelist and nodelist index file names
	 */
	
	if( *name == DIRSEPCHR )
	     /* if nodelist name contains full path */
	{
	     logerr("nodelist.c: you shold specify only filename, without path");
	     exit(255);
	}
	     /* if only just a filename */
	else
	{
	     /* checking, if nodelist name contains mask (see
	      * example config for details) */
	     /* If nodelist extension == "999" then we assume that it
	      * is a mask, and try to find the latest nodelist for
	      * this mask. If we find it, then we change "name"
	      * parameter.
	      * If not, we do logeer
	      *
	      * TODO: as in qico, do mask-search not case-sentesive
	      */
	if( strcmp(name+strlen(name)-4, ".999") == 0 )
	{
	     char tmpseek[MAX_NAME];
	     char tmpname[MAX_NAME];
	     struct dirent *ndir;
	     DIR *ndirstream;
	     if( (ndirstream = opendir(dir)) == NULL )
	     {
		  log("error opening nodelist directory: %s", dir);
	     }
	     else
	     {
		  strncpy(tmpname, name, sizeof(tmpname));
		  struct stat ndfile;
		  time_t lasttime = 0;
		  while( (ndir = readdir(ndirstream)) )
		  {
		       strncpy(tmpseek, ndir->d_name, sizeof(tmpseek));
		       if ( strlen(tmpseek) < 3 )
			    continue;

		       if( strlen(tmpseek) == strlen(tmpname) )
		       {
			    if( (strncmp(tmpseek, tmpname, (strlen(tmpseek)-3) ) == 0) )
			    {
				 if( stat(tmpseek, &ndfile) )
				 {
				      if( ndfile.st_ctime > lasttime )
				      {
					   lasttime = ndfile.st_ctime;
					   strncpy(name, tmpseek, MAX_NAME-strlen(tmpseek));
				      }
				 }
			    }
		       }
		  }
	     }
	     closedir(ndirstream);
	}

	if( strcmp(name+strlen(name)-4, ".999") == 0 )
	     /* we haven`t found any nodelist for this mask */
	{
	     logerr("No nodelist found for this mask: %s", name);
	}

		strnxcpy(tmp.name_nodelist, dir, sizeof(tmp.name_nodelist));
		strnxcat(tmp.name_nodelist, name, sizeof(tmp.name_nodelist));
		strnxcpy(tmp.name_index, tmp.name_nodelist, sizeof(tmp.name_index));
	}
	strnxcat(tmp.name_index, ".index", sizeof(tmp.name_index));
	
	DEB((D_NODELIST, "nodelist_open: nodelist name = \"%s\"", tmp.name_nodelist));
	DEB((D_NODELIST, "nodelist_open: nodelist index = \"%s\"", tmp.name_index));
	DEB((D_NODELIST, "nodelist_open: open mode = \"%s\"", openmode));
	
	/*
	 * Try to open and lock nodelist
	 */	
	if( (tmp.fp_nodelist = file_open(tmp.name_nodelist, "r")) == NULL )
	  {
		logerr("cannot open nodelist \"%s\"", tmp.name_nodelist);
		return NULL;
	}
	
	/*
	 * Try to open and lock nodelist index
	 */	
	if( (tmp.fp_index = file_open(tmp.name_index, openmode)) == NULL )
	{
		logerr("cannot open nodelist index \"%s\"", tmp.name_index);
		fclose(tmp.fp_nodelist);
		return NULL;
	}
	
	/*
	 * If we open nodelist for reading then we should check that
	 * nodelist index has correct header and up to date
	 */
	if( mode == NODELIST_READ )
	{
		if( nodelist_checkheader(&tmp) == -1 )
		{
			file_close(tmp.fp_nodelist);
			file_close(tmp.fp_index);
			return NULL;
		}
	}
	
	return xmemcpy(&tmp, sizeof(s_nodelist));
}

/*****************************************************************************
 * Check nodelist index header for valid nodelist date and size
 *
 * Arguments:
 * 	nlp       opened nodelist
 *
 * Return value:
 * 	zero value if header is correct, and -1 if not
 */
int nodelist_checkheader(s_nodelist *nlp)
{
	unsigned long nltime;
	unsigned long nlsize;
	struct stat nlstat;
	char buffer[NODELIST_HDRSIZE];

	if( fread(buffer, sizeof(buffer), 1, nlp->fp_index) != 1 )
	{
		logerr("cannot read header from nodelist index \"%s\"", nlp->name_index);
		return -1;
	}
	
	if( fstat(fileno(nlp->fp_nodelist), &nlstat) == -1 )
	{
		logerr("cannot stat nodelist \"%s\"", nlp->name_nodelist);
		return -1;
	}
	
	nltime = buffer_getlong(buffer + 0);
	nlsize = buffer_getlong(buffer + 4);
	
	if( (unsigned long)nlstat.st_mtime != nltime )
	{
		log("invalid nodelist index: incorrect nodelist date %ld (expected %ld)",
			(long)nlstat.st_mtime, (long)nltime);
		return -1;
	}

	if( (unsigned long)nlstat.st_size != nlsize )
	{
		log("invalid nodelist index: incorrect nodelist size %ld (expected %ld)",
			(long)nlstat.st_size, (long)nlsize);
		return -1;
	}
	
	return 0;
}

/*****************************************************************************
 * Create/update nodelist index header. Nodelist must be opened for
 * writing. Now nodelist index header contain nodelist modification
 * time and nodelist size.
 *
 * Arguments:
 * 	nlp       opened nodelist
 *
 * Return value:
 * 	zero value on success, and -1 at errors
 */
int nodelist_createheader(s_nodelist *nlp)
{
	struct stat nlstat;
	char hdrbuf[NODELIST_HDRSIZE];

	memset(&hdrbuf, '\0', sizeof(hdrbuf));

	if( fstat(fileno(nlp->fp_nodelist), &nlstat) == -1 )
	{
		logerr("cannot stat nodelist \"%s\"", nlp->name_nodelist);
		return -1;
	}

	buffer_putlong(hdrbuf + 0, nlstat.st_mtime);
	buffer_putlong(hdrbuf + 4, nlstat.st_size);
	
	if( fseek(nlp->fp_index, 0L, SEEK_SET) == -1 )
	{
		logerr("cannot seek to zero offset of index");
		return -1;
	}

	if( fwrite(hdrbuf, sizeof(hdrbuf), 1, nlp->fp_index) != 1 )
	{
		logerr("cannot write nodelist index header");
		return -1;
	}

	return 0;
}

/*****************************************************************************
 * Close nodelist and nodelist index files
 *
 * Arguments:
 * 	nlp       pointer to the opened nodelist
 *
 * Return value:
 * 	Zero value if close was successful, and non-zero if not
 */
int nodelist_close(s_nodelist *nlp)
{
	int rc = 0;

	ASSERT(nlp && nlp->fp_nodelist && nlp->fp_index);
	
	if( nlp->fp_nodelist && file_close(nlp->fp_nodelist) )
		logerr("cannot close nodelist \"%s\"", nlp->name_nodelist);
	
	if( nlp->fp_index && file_close(nlp->fp_index) )
	{
		logerr("cannot close nodelist index \"%s\"", nlp->name_index);
		rc = 1;
	}
	
	free(nlp);
	
	return rc;
}

int nodelist_putindex(s_nodelist *nlp, const s_bni *bni)
{
	char buffer[NODELIST_ENTRYSIZE];
	
	ASSERT(nlp && nlp->fp_index);
	
	buffer_putint(buffer + 0, bni->zone);
	buffer_putint(buffer + 2, bni->net);
	buffer_putint(buffer + 4, bni->node);
	buffer_putint(buffer + 6, bni->point);
	buffer_putint(buffer + 8, bni->hub);
	buffer_putlong(buffer + 10, bni->offset);
	
	if( fwrite(buffer, sizeof(buffer), 1, nlp->fp_index) != 1 )
	{
		logerr("error writing nodelist index file \"%s\"", nlp->name_index);
		return -1;
	}
	
	return 0;
}

int nodelist_findindex(s_nodelist *nlp, s_bni *bni, s_faddr addr)
{
	long readitems;
	char buffer[NODELIST_ENTRYSIZE * NODELIST_READAHEAD];
	char *p;
	
	if( fseek(nlp->fp_index, NODELIST_HDRSIZE, SEEK_SET) ) return -1;
	
	while(1)
	{
		if( (readitems = fread(buffer, NODELIST_ENTRYSIZE, NODELIST_READAHEAD, nlp->fp_index)) > 0 )
		{
			for( p = buffer; readitems > 0; readitems-- )
			{
				if( buffer_getint(p + 0) == addr.zone
				 && buffer_getint(p + 2) == addr.net
				 && buffer_getint(p + 4) == addr.node
				 && buffer_getint(p + 6) == addr.point )
				{
					bni->zone   = buffer_getint(p + 0);
					bni->net    = buffer_getint(p + 2);
					bni->node   = buffer_getint(p + 4);
					bni->point  = buffer_getint(p + 6);
					bni->hub    = buffer_getint(p + 8);
					bni->offset = buffer_getlong(p + 10);
					return 0;
				}
				p += NODELIST_ENTRYSIZE;
			}
		} else
			return -1;
	}
	
	return -1;
}

int nodelist_getstr(s_nodelist *nlp, size_t offset, char *buffer, size_t buflen)
{
	ASSERT(nlp && nlp->fp_nodelist);
	
	if( fseek(nlp->fp_nodelist, offset, SEEK_SET) == 0
	 && fgets(buffer, buflen, nlp->fp_nodelist) )
	{
		string_chomp(buffer);
		return 0;
	}
	
	return -1;
}

int nodelist_lookup_string(char *buffer, size_t buflen, s_faddr addr)
{
	s_bni bni;
	s_cval_entry *ptrl;
	s_nodelist *nlp;
	const char *ndldir = NULL;
	
	ndldir = conf_string(cf_nodelist_directory);
	
	/*
	 * Try all nodelists with matching address mask
	 */
	for( ptrl = conf_first(cf_nodelist); ptrl; ptrl = conf_next(ptrl) )
	{
		if( ftn_addrcomp_mask(addr, ptrl->d.falist.addr) == 0 )
		{
			if( (nlp = nodelist_open(ndldir, ptrl->d.falist.what, NODELIST_READ)) )
			{
				int rc = nodelist_findindex(nlp, &bni, addr);
				
				if( !rc )
					rc = nodelist_getstr(nlp, bni.offset, buffer, buflen);
				
				nodelist_close(nlp);
				
				if( !rc )
					return 0;
			}
		}
	}
	
	return -1;
}

int nodelist_lookup(s_node *node, s_faddr addr)
{
	char buf[512];
	char abuf[BF_MAXADDRSTR+1];
	
	nodelist_initnode(node, addr);
	
	if( nodelist_lookup_string(buf, sizeof(buf), addr) == 0 )
	{
		node->listed = TRUE;
		if( nodelist_parsestring(node, buf) == -1 )
		{
			log("invalid nodelist string for address %s",
				ftn_addrstr(abuf, addr));
		}
		return 0;
	}
	
	return -1;
}

void nodelist_initnode(s_node *node, s_faddr addr)
{
	memset(node, '\0', sizeof(s_node));
	
	node->addr = addr;
	strcpy(node->name, "<none>");
	strcpy(node->sysop, "<none>");
	strcpy(node->location, "<none>");
	strcpy(node->phone, "<none>");
}

