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

struct fnctab {
	const char *find;
	const char *repl;
} fnctab[] = {
	{ ".tar.gz",    ".tgz" },
	{ ".tar.bz2",   ".tbz" },
	{ ".html",      ".htm" },
	{ ".desc",      ".dsc" },
	{ NULL,          NULL  }
};

/*****************************************************************************
 * Lock whole file for reading or writing
 *
 * Arguments:
 * 	fp        pointer to the opened file FILE structure
 * 	exclusive lock file for exclusive or shared access
 *
 * Return value:
 * 	zero value on success and -1 on errors
 */
int file_lock(FILE *fp, bool exclusive)
{
#ifdef USE_FLOCK
	if( flock(fileno(fp), (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1 )
		return -1;
#else
	struct flock flck;
	
	memset(&flck, '\0', sizeof(struct flock));
	flck.l_whence = SEEK_SET;
	flck.l_start = 0;
	flck.l_len = 0;
	flck.l_type = exclusive ? F_WRLCK : F_RDLCK;
	
	if( fcntl(fileno(fp), F_SETLK, &flck) == -1 )
		return -1;
#endif
	
	return 0;
}

/*****************************************************************************
 * Unlock file
 *
 * Arguments:
 * 	fp        pointer to the opened file FILE structure
 *
 * Return value:
 * 	None
 */
void file_unlock(FILE *fp)
{
#ifdef USE_FLOCK
	(void)flock(fileno(fp), LOCK_UN | LOCK_NB)
#else
	struct flock flck;
	
	memset(&flck, '\0', sizeof(struct flock));
	flck.l_whence = SEEK_SET;
	flck.l_start  = 0;
	flck.l_len    = 0;
	flck.l_type = F_UNLCK;
	
	(void)fcntl(fileno(fp), F_SETLK, &flck);
#endif
}

/*****************************************************************************
 * Lock whole file for reading or writing, do some number of tries before
 * giving up
 *
 * Arguments:
 * 	fp        pointer to the opened file FILE structure
 * 	exclusive lock file for exclusive or shared access
 *
 * Return value:
 * 	zero value on success and -1 on errors
 */
int file_lock_wait(FILE *fp, bool exclusive)
{
	int tries;
	
	for( tries = 0; tries < 50; tries++ )
	{
		if( file_lock(fp, exclusive) == 0 )
			return 0;
		usleep(200);
	}
	
	return -1;
}

bool file_name_issafe(int ch)
{
	if( ch == '!' ) return TRUE;
	if( ch == '"' ) return TRUE;
	if( ch == '#' ) return TRUE;
	if( ch == '$' ) return TRUE;
	if( ch == '&' ) return TRUE;
	if( ch == '+' ) return TRUE;
	if( ch == ',' ) return TRUE;
	if( ch == '-' ) return TRUE;
	if( ch == '.' ) return TRUE;
	if( ch == '@' ) return TRUE;
	if( ch == '_' ) return TRUE;
	if( ch == '^' ) return TRUE;
	if( ch == '~' ) return TRUE;
	if( ch > 47  && ch <  58 ) return TRUE; /* '0'-'9' */
	if( ch > 64  && ch <  91 ) return TRUE; /* 'A'-'Z' */
	if( ch > 96  && ch < 123 ) return TRUE; /* 'a'-'z' */
	if( ch > 127 && ch < 256 ) return TRUE; /* Hmm. :) */
	
	return FALSE;
}

char *file_name_makesafe(char *filename)
{
	char *p;
	
	for( p = filename; *p; p++ )
		if( !file_name_issafe((unsigned char)(*p)) ) *p = '_';
	
	return filename;
}

char *file_getname(char *filename)
{
	char *p = strrchr(filename, '/');
	return p ? p + 1 : filename;
}

char *file_gettmp(void)
{
	char *tmp = xstrcpy("/tmp/bforce-XXXXXX");
	char *res = mktemp(tmp);

	if( !res )
		free(tmp);

	return res;
}

char *file_get_dos_name(char *buffer, const char *filename)
{
	char *copy = xstrcpy(filename);
	char *p;
	int   i;
	int   extension_length = 0;

	for( i = 0; fnctab[i].find; i++ )
	{
		if( (p = strstr(copy, fnctab[i].find)) )
		{
			*p = '\0';
			strcat(copy, fnctab[i].repl);
			strcat(copy, p + strlen(fnctab[i].find));
			break;
		}
	}
	
	/*
	 * Make sure, there is no '.' characters
	 * except extension separator point
	 */
	for( p = copy + strlen(copy) - 1; p >= copy; p-- )
	{
		if( (unsigned char)(*p) <= ' ' || *p == '*' || *p == '?' )
			*p = '_';
		else if( *p == '.' )
		{
			if( 4 >= strlen(p) && strlen(p) > 1 && !extension_length )
				extension_length = strlen(p + 1);
			else
				*p = '_';
		}
	}
		
	if( (extension_length >  0 && strlen(copy) <= 9 + extension_length)
	 || (extension_length == 0 && strlen(copy) <= 8) )
		strcpy(buffer, copy);
	else
	{
		strncpy(buffer, copy, 5);
		if( extension_length > 0 )
			strncpy(buffer + 6, copy + strlen(copy) - 3 - extension_length, 6);
		else
			strncpy(buffer + 6, copy + strlen(copy) - 2, 2);
		buffer[5] = '~';
		buffer[12] = '\0';
	}

	free(copy); copy = NULL;
	
	/*
	 * Make it lower case
	 */
	string_tolower(buffer);
	
	if( !extension_length )
		buffer[8] = '\0';

	return buffer;
}

bool is_directory(const char *dirname)
{
	struct stat st;

	if( stat(dirname, &st) == -1 )
		return FALSE;
	
	return S_ISDIR(st.st_mode) ? TRUE : FALSE;
}

bool is_regfile(const char *filename)
{
	struct stat st;

	if( stat(filename, &st) == -1 )
		return FALSE;

	return S_ISREG(st.st_mode) ? TRUE : FALSE;
}

int directory_create(const char *dirname, mode_t access_mode)
{
	char tmpname[BFORCE_MAX_PATH+1];
	char *p = tmpname;
	bool eol = FALSE;
	
	strnxcpy(tmpname, dirname, sizeof(tmpname));
	
	while( *p && !eol )
	{
		while( *p && *p != '/' ) ++p;
		
		if( *p )
			*p = '\0';
		else
			eol = TRUE;
		
		if( !is_directory(tmpname) )
		{
			if( mkdir(tmpname, access_mode) == -1 && errno != EEXIST )
				return -1;
			(void)chmod(tmpname, access_mode);
		}
		
		if( !eol )
			*p++ = '/';
	}

	return 0;
}

FILE *file_open(const char *path, const char *mode)
{
	FILE *stream;
	bool is_write;

	ASSERT(path);
	ASSERT(mode && (*mode == 'r' || *mode == 'w' || *mode == 'a'));

	stream = fopen(path, mode);
	if( !stream )
		return NULL;
	
	is_write = (*mode != 'r') ? TRUE : FALSE;
	
	/* Try to lock it */
	if( file_lock(stream, is_write) == 0 )
		return stream;

	/* If we can't physically lock file, just send it as is */
	if ( errno == EOPNOTSUPP || errno == EINVAL )
		return stream;
	
	/* Do second try */
	if( file_lock_wait(stream, is_write) == 0 )
		return stream;
	
	fclose(stream);
	return NULL;
}

int file_close(FILE *stream)
{
	ASSERT(stream);

	file_unlock(stream);
	return fclose(stream);
}

