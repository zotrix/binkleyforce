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
#include "freq.h"

static void deinit_reqlist(s_reqlist *dest);
static void deinit_frlist(s_frlist *dest);

/* ------------------------------------------------------------------------- */
/* Log requested files, passwords and other specified information            */
/* ------------------------------------------------------------------------- */
static void req_logreqlist(s_reqlist *reqlist)
{
	s_reqlist *rlist;
	char *p = NULL;
	char buf[100];

	for( rlist = reqlist; rlist; rlist = rlist->next )
	{
		if( rlist->fmask == NULL ) continue;
		
		p = xstrcpy("requested \"");
		p = xstrcat(p, rlist->fmask);
		p = xstrcat(p, "\"");
		if( rlist->passwd )
		{
			p = xstrcat(p, ", password \"");
			p = xstrcat(p, rlist->passwd);
			p = xstrcat(p, "\"");
		}
		if( rlist->newer )
		{
			p = xstrcat(p, ", newer \"");
			p = xstrcat(p, time_string_long(buf, sizeof(buf), rlist->newer));
			p = xstrcat(p, "\"");
		}
		if( rlist->older )
		{
			p = xstrcat(p, ", older \"");
			p = xstrcat(p, time_string_long(buf, sizeof(buf), rlist->older));
			p = xstrcat(p, "\"");
		}
		log("FREQ: %s", string_printable(p));
		if( p ) { free(p); p = NULL; }
	}
}

/* ------------------------------------------------------------------------- */
/* Read files containing list of public dirs and file aliases                */
/* ------------------------------------------------------------------------- */
static void req_readfrlist(char *fname, s_frlist **frlist, int magic)
{
	FILE *fp;
	s_frlist **ptrl;
	char sbuf[BF_MAXREQLINE+1];
	char *magc = NULL;
	char *path = NULL;
	char *pwd  = NULL;
	char *n    = NULL;
	
	DEB((D_FREQ, "req_readfrlist: reading \"%s\", magic = %d", fname, magic));
	
	if( (fp = file_open(fname, "r")) == NULL )
	{
		logerr("FREQ: can't open dirlist \"%s\"", fname);
		return;
	}
	
	for( ptrl = frlist; *ptrl; ptrl = &(*ptrl)->next )
	{
		/* EMPTY LOOP */
	}
	
	while( fgets(sbuf, sizeof(sbuf), fp) )
	{
		string_chomp(sbuf);

		if( *sbuf == '#' ) continue;
		
		if( magic )
		{
			magc = string_token(sbuf, &n, NULL, 1);
			path = string_token(NULL, &n, NULL, 1);
		}
		else
		{
			path = string_token(sbuf, &n, NULL, 1);
		}
		
		pwd = string_token(NULL, &n, NULL, 1);
		
		/* make sure it is password */
		if( pwd && *pwd == '!' ) pwd++; else pwd = NULL;
		
		if( path && *path && (!magic || (magc && *magc)) )
		{
			DEB((D_FREQ, "req_readfrlist: add path = \"%s\", magic = \"%s\", passwd = \"%s\"",
				path, magc, pwd));
				
			(*ptrl) = (s_frlist*)xmalloc(sizeof(s_frlist));
			memset(*ptrl, '\0', sizeof(s_frlist));
			
			if( path && *path ) (*ptrl)->path   = xstrcpy(path);
			if( !magic && *(path + strlen(path) - 1) != DIRSEPCHR )
			{
				/* add trailing DIRSEP ('/' or ..) to paths */
				(*ptrl)->path = xstrcat((*ptrl)->path, DIRSEPSTR);
			}
			if( magc && *magc ) (*ptrl)->magic  = xstrcpy(magc);
			if( pwd  && *pwd  ) (*ptrl)->passwd = xstrcpy(pwd);
			ptrl = &(*ptrl)->next;
		}
	}
	
	file_close(fp);
}

/* ------------------------------------------------------------------------- */
/* Add files to send files list, check limits. Return non-zero value         */
/* if can't send more files.                                                 */
/* ------------------------------------------------------------------------- */
static int req_addfile(char *fname, s_freq *freq)
{
	struct stat st;

	DEB((D_FREQ, "req_addfile: try to add \"%s\"", fname));
	
	if( stat(fname, &st) )
	{
		logerr("FREQ: can't stat requested file \"%s\"", fname);
	}
	else
	{
		if( S_ISREG(st.st_mode) == 0 )
		{
			log("FREQ: requested file isn't regular \"%s\"", fname);
		}
		else if( (freq->sizelimit > 0)
		      && ((freq->fsize + st.st_size) > freq->sizelimit) )
		{
			DEB((D_FREQ, "req_addfile:sending FREQ file \"%s\" exceed size limit", fname));
		}
		else
		{
			freq->fnumber += 1;
			freq->fsize += st.st_size;
			
			log("FREQ: send \"%s\", %d byte(s)", fname, st.st_size);
			
			(*freq->flast) = (s_filelist*)xmalloc(sizeof(s_filelist));
			memset(*freq->flast, '\0', sizeof(s_filelist));
			(*freq->flast)->fname  = fname ? xstrcpy(fname) : NULL;
			(*freq->flast)->size   = st.st_size;
			(*freq->flast)->type   = TYPE_REQANSW;
			(*freq->flast)->status = STATUS_WILLSEND;
			(*freq->flast)->action = ACTION_NOTHING;
			freq->flast = &(*freq->flast)->next;
		}
	}
	
	if( (freq->fileslimit > 0) && (freq->fnumber >= freq->fileslimit) )
	{
		log("FREQ: reached files number limit (%d file(s))", freq->fileslimit);
		return(1);
	}
	return(0);
}

/* ------------------------------------------------------------------------- */
/* Return non-zero if got incorrect password for password protected file     */
/* ------------------------------------------------------------------------- */
static int checkpasswd(char *ourpwd, char *gotpwd)
{
	if( ourpwd == NULL || *ourpwd == '\0' ) return(0);
	if( gotpwd && strcasecmp(ourpwd, gotpwd) == 0 ) return(0);

	return(1);
}

/* ------------------------------------------------------------------------- */
/* Run external (SRIF compartible) FREQ processor                            */
/* ------------------------------------------------------------------------- */
static void req_proc_ext(s_freq *freq, char *reqname)
{
	char srfname[L_tmpnam+5];
	char rspname[L_tmpnam+5];
	char *comline = NULL;
	
	if( tmpnam(srfname) )
	{
		strncpy(rspname, srfname, L_tmpnam+4);
		rspname[L_tmpnam+4] = '\0';
		strcat(srfname, ".srf");
		strcat(rspname, ".rsp");
		if( req_createsrif(srfname, reqname, rspname) == 0 )
		{
			comline = xstrcpy(freq->srifproc);
			comline = xstrcat(comline, " ");
			comline = xstrcat(comline, srfname);
			if( session_run_command(comline) == 0 )
			{
				req_addfilelist(rspname, freq);
			}
			unlink(srfname);
			unlink(rspname);
			if( comline ) { free(comline); }
		}
	}
	else
	{
		logerr("FREQ: can't generate temp name for SRIF");
	}
}

/* ------------------------------------------------------------------------- */
/* Run internal FREQ processor                                               */
/* ------------------------------------------------------------------------- */
static void req_proc_int(s_freq *freq)
{
	s_reqlist *rlist;
	s_frlist *frl;
	struct dirent *dirent;
	DIR *dirp;
	char *fname = NULL;
	int stop = 0;	/* Stop processing FREQs */
	int msg  = 0;	/* Show "incorrect password" message only once */

	for( frl = freq->frlist; !stop && frl; frl = frl->next )
	{
		if( frl->magic && *frl->magic )
		{
			/* MAGIC */
			DEB((D_FREQ, "req_proc_int: checking our magic \"%s\"", frl->magic));
			
			for( rlist = freq->reqlist; rlist; rlist = rlist->next )
			{
				if( !strcasecmp(frl->magic, rlist->fmask) )
				{
					if( 0 == checkpasswd(frl->passwd, rlist->passwd) )
					{
						rlist->skip = 1;
						stop = req_addfile(frl->path, freq);
					}
					else
					{
						log("FREQ: incorrect password for magic \"%s\"", frl->magic);
					}
					break;
				}
			}
			if( stop == 0 )
			{
				/* Check - may be there is no more */
				/* requests - so break it out      */
				for( rlist = freq->reqlist; rlist; rlist = rlist->next )
				{
					if( rlist->skip == 0 ) break;
				}
				stop = (rlist == NULL);
			}
		}
		else if( frl->path && *frl->path )
		{
			/* FREQ DIR */
			if( (dirp = opendir(frl->path)) == NULL )
			{
				logerr("FREQ: can't open freq dir \"%s\"", frl->path);
			}
			else
			{
				/* REGULAR DIR */
				DEB((D_FREQ, "req_proc_int: checking dir \"%s\"", frl->path));
			
				while( !stop && (dirent = readdir(dirp)) )
				{
					if( *dirent->d_name == '.' ) continue;
					for( rlist = freq->reqlist; rlist; rlist = rlist->next )
					{
						if( rlist->skip == 0 
						 && 0 == strcasemask(dirent->d_name, rlist->fmask) )
						{
							if( 0 == checkpasswd(frl->passwd, rlist->passwd) )
							{
								fname = xstrcpy(frl->path);
								fname = xstrcat(fname, dirent->d_name);
								stop = req_addfile(fname, freq);
								free(fname);
							}
							else if( msg == 0 )
							{
								log("FREQ: incorrect password");
								++msg;
							}
							break;
						}
					}
				}
				closedir(dirp);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Start FREQs processing, run int. or ext. processor                        */
/* ------------------------------------------------------------------------- */
void req_proc(char *reqname, s_filelist **filelist)
{
	char *p = NULL;
	time_t timer = time(NULL);
	s_freq freq;
	
	/* Init FREQ information structure */
	memset(&freq, '\0', sizeof(s_freq));
	freq.fileslimit = conf_number(cf_freq_limit_number);
	freq.sizelimit  = conf_number(cf_freq_limit_size);
	freq.filelist   = filelist;
	freq.flast      = filelist;
	
	if( (p = conf_string(cf_freq_srif_command)) && *p )
	{
		/* Try external (SRIF) FREQ processor */
		log("FREQ: starting external processor");
		
		freq.srifproc = xstrcpy(p);
		
		req_proc_ext(&freq, reqname);
		
		if( freq.srifproc )
			free(freq.srifproc);
	}
	else
	{
		/* Run internal FREQ processor */
		log("FREQ: starting internal processor");
		
		if( req_readwazooreq(reqname, &freq.reqlist) == 0 )
		{
			req_logreqlist(freq.reqlist);
			
			if( (p = conf_string(cf_freq_alias_list)) && *p )
				req_readfrlist(p, &freq.frlist, 1);
			
			if( (p = conf_string(cf_freq_dir_list)) && *p )
				req_readfrlist(p, &freq.frlist, 0);
			
			if( freq.frlist )
			{
				req_proc_int(&freq);
				deinit_frlist(freq.frlist);
			}
			deinit_reqlist(freq.reqlist);
		}
	}
	
	log("FREQ: stat %d file(s), %d byte(s), %d second(s)",
		freq.fnumber, freq.fsize, time(NULL)-timer);
	
#ifdef DEBUG
	if( freq.filelist )
		log_filelist(*freq.filelist);
#endif	
}

static void deinit_reqlist(s_reqlist *dest)
{
	s_reqlist *ptrl, *next;
	
	for( ptrl = dest; ptrl; ptrl = next )
	{
		next = ptrl->next;
		if( ptrl->fmask  ) free(ptrl->fmask);
		if( ptrl->passwd ) free(ptrl->passwd);
		free(ptrl);
	}
}

static void deinit_frlist(s_frlist *dest)
{
	s_frlist *ptrl, *next;
	
	for( ptrl = dest; ptrl; ptrl = next )
	{
		next = ptrl->next;
		if( ptrl->magic  ) free(ptrl->magic);
		if( ptrl->path   ) free(ptrl->path);
		if( ptrl->passwd ) free(ptrl->passwd);
		if( ptrl->expr   ) free(ptrl->expr);
		free(ptrl);
	}
}
