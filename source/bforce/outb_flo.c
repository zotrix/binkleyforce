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

char *flo_translate(char *buffer, size_t buflen)
{
	char *p;
	s_cval_entry *ptrl;

	ASSERT(buffer && buflen);
	
	for( ptrl = conf_first(cf_flo_translate); ptrl;
	     ptrl = conf_next(ptrl) )
	{
		ASSERT(ptrl->d.translate.find);
		
		if( (p = strstr(buffer, ptrl->d.translate.find)) )
		{
			char *new = string_translate(buffer,
					ptrl->d.translate.find,
					ptrl->d.translate.repl);
			if( new )
			{
				strnxcpy(buffer, new, buflen);
				free(new);
			}
/*
 * I don't want to test it
 */
#ifdef XXX
			int find_len = strlen(ptrl->d.translate.find);
			
			if( ptrl->d.translate.repl )
			{
				int repl_len = strlen(ptrl->d.translate.repl);
				if( repl_len < find_len )
				{
					memcpy(p, ptrl->d.translate.repl, repl_len);
					memmove(p+repl_len, p+find_len, strlen(p+find_len)+1);
				}
				else if( repl_len == find_len )
				{
					memcpy(p, ptrl->d.translate.repl, repl_len);
				}
				else /* ( repl_len > find_len ) */
				{
					/* TODO =) */
				}
			}
			else
				memmove(p, p+find_len, strlen(p+find_len)+1);
#endif /* XXX */
		}
	}

	return buffer;
}

s_FLO *flo_open(const char *path, const char *mode)
{
	FILE *fp;
	s_FLO *FLO;
	
	if( (fp = file_open(path, mode)) == NULL )
		return NULL;

	FLO = (s_FLO *)xmalloc(sizeof(s_FLO));
	memset(FLO, 0, sizeof(s_FLO));
	FLO->fp = fp;

	return FLO;
}

int flo_next(s_FLO *FLO)
{
restart:
	FLO->att_offs = ftell(FLO->fp);
	if( FLO->att_offs == -1 )
		return -1;

	if( fgets(FLO->att_path, BFORCE_MAX_PATH, FLO->fp) == NULL )
		return -1;
	
	if( string_is_empty(FLO->att_path) || *FLO->att_path == '~' )
		goto restart;
	
	string_chomp(FLO->att_path);

	switch(*FLO->att_path) {
	case '@':
		FLO->att_action = ACTION_NOTHING;
		memmove(FLO->att_path, FLO->att_path+1, strlen(FLO->att_path+1)+1);
		break;
	case '^':
	case '-':
		FLO->att_action = ACTION_UNLINK;
		memmove(FLO->att_path, FLO->att_path+1, strlen(FLO->att_path+1)+1);
		break;
	case '#':
		FLO->att_action = ACTION_TRUNCATE;
		memmove(FLO->att_path, FLO->att_path+1, strlen(FLO->att_path+1)+1);
		break;
	default:
		FLO->att_action = ACTION_NOTHING;
	}

	flo_translate(FLO->att_path, BFORCE_MAX_PATH);
	
	return 0;
}

int flo_mark_sent(s_FLO *FLO)
{
	long myoffs = ftell(FLO->fp);

	if( fseek(FLO->fp, (long)FLO->att_offs, SEEK_SET) == -1
	 || fputc('~', FLO->fp) == EOF )
		return -1;
	
	if( myoffs >= 0 )
		fseek(FLO->fp, myoffs, SEEK_SET);

	return 0;
}

int flo_eof(s_FLO *FLO)
{
	return feof(FLO->fp);
}

int flo_close(s_FLO *FLO)
{
	int rc = file_close(FLO->fp);
	free(FLO);
	return rc;
}
/*
 * Return "TRUE" if no more files to send left in the FLO
 */
bool out_flo_isempty(const char *path)
{
	bool rc;
	s_FLO *FLO;
	
	if( (FLO = flo_open(path, "r")) == NULL )
		return FALSE;

	while( flo_next(FLO) == 0 )
	{
		if( access(FLO->att_path, F_OK) == 0 )
			return FALSE;
	}
	rc = flo_eof(FLO) ? TRUE : FALSE;

	flo_close(FLO);

	return rc;
}

/*
 * Mark the file name in FLO file as sent ( with '~' prefix )
 */
int out_flo_marksent(const char *fname, const char *floname)
{
	int rc = -1;
	s_FLO *FLO;
	
	if( (FLO = flo_open(floname, "r+")) == NULL )
	{
		logerr("can't open flo file \"%s\"", floname);
		return -1;
	}
	
	while( flo_next(FLO) == 0 )
	{
		if( strcmp(fname, FLO->att_path) == 0 )
		{
			if( flo_mark_sent(FLO) == -1 )
				logerr("can't mark \"%s\" as sent in \"%s\"",
						fname, floname);
			else
				rc = 0;
			break;
		}
	}

	if( rc == -1 && flo_eof(FLO) )
		rc = 0;
	
	flo_close(FLO);
	
	return rc;
}

/*
 * Unlink empty .?lo files
 */
int out_flo_unlinkempty(const s_flofile *flotab, int flonum)
{
	int i;
	
	for( i = 0; i < flonum; i++ )
	{
		if( out_flo_isempty(flotab[i].fname) )
		{
			if( unlink(flotab[i].fname) == -1 )
				logerr("cannot unlink empty flo file \"%s\"",
						flotab[i].fname);
		}
	}

	return 0;
}
