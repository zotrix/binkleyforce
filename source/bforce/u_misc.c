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

/*
 * Array of mask aliases, you may add new values if you need
 */
struct magfname {
	const char *alias;
	const char *masks;
} magfname[] = {
	{ "netmail",	"*.pkt *.out *.cut *.hut *.dut *.iut"       },
	{ "arcmail",	"*.su? *.mo? *.tu? *.we? *.th? *.fr? *.sa?" },
	{ "ticfile",	"*.tic"                                     },
	{ "archive",	"*.tgz *.bz2 *.zip *.rar *.arj *.lha *.ha"  },
	{ NULL,         NULL                                        }
};

/*****************************************************************************
 * Allocate memory, check for errors
 *
 * Arguments:
 * 	size      number of bytes to allocate
 *
 * Return value:
 * 	Pointer to the allocated memory
 */
void *xmalloc(size_t size)
{
	char *tmp;
	
	if( size <= 0 )
	{
		log("requested to allocate %ld bytes", (long)size);
		size = 1;
	}
	
	if( (tmp = (char *)malloc(size)) == NULL )
	{
		log("failed to allocate %ld bytes -> abort", (long)size);
		abort();
	}
	
	return tmp;
}

/*****************************************************************************
 * Change the size of allready allocated memory
 *
 * Arguments:
 * 	buf       pointer to the memory block which size we want change
 * 	size      new block size
 *
 * Return value:
 * 	Pointer to the reallocated memory
 */
void *xrealloc(void *buf, size_t size)
{
	char *tmp;
	
	if( size <= 0 )
	{
		tmp = NULL;
		log("requested to reallocate %ld bytes", (long)size);
		if( buf ) free(buf);
	}
	else if( (tmp = (char*)(buf ? realloc(buf, size) : malloc(size))) == NULL )
	{
		log("failed to reallocate %ld bytes -> abort", size);
		abort();
	}
	
	return tmp;
}

/*****************************************************************************
 * Make copy of memory block
 *
 * Arguments:
 * 	buffer    pointer to the memory block which we want to duplicate
 * 	size      memory block size
 *
 * Return value:
 * 	Pointer to the memory block copy (must be freed)
 */
void *xmemcpy(const void *buffer, size_t buflen)
{
	void *ptr = xmalloc(buflen);
	
	memcpy(ptr, buffer, buflen);
	
	return ptr;
}

/*****************************************************************************
 * Compare string for matching to the mask. Special characters for the mask:
 *   '?' - any one chracter (except '\0');
 *   '*' - any group of chracters (including '\0');
 *   '\' - next chracter will be compared directly (including '?' and '*')
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 * 	mask      pointer to the null-terminated mask
 *
 * Return value:
 * 	zero value if string matchs pattern, and non-zero if not
 */
int strcasemask(const char *str, const char *mask)
{
	const char *s, *m;
	char c;
	
	ASSERT(str != NULL && mask != NULL);
	
	s = str;
	m = mask;
	
	while( *s )
	{
		switch( c = *m++ ) {
		case '\0':
			return 1;
			
		case '\\':
			if( *m )
			{
				if( toupper(*m++) != toupper(*s++) )
					return 1;
			}
			else if( *s++ == '\\' && *s == '\0' )
				return 0;
			else
				return 1;
			break;
			
		case '?':
			++s;
			break;
			
		case '*':
			if( *m )
			{
				do {
					/* anti stack overflow */
					while( *m == '*' ) m++;
					
					if( *m != '\\' && *m != '?' )
						while( *s && toupper(*s) != toupper(*m) ) ++s;
					
					if( *s && !strcasemask(s, m) )
							return 0;
				} while( *s++ );
				
				return 1;
			}
			return 0;
			
		default:
			if( toupper(c) != toupper(*s++) )
				return 1;
		}
	}
	
	return( *s != '\0' || *m != '\0' );
}

/*****************************************************************************
 * Check whether masks list (e.g. "*.zip *.rar *.tic") conforms to the file
 * name. For the masks description look string_pattern() info.
 *
 * Arguments:
 * 	masks     pointer to the null-terminated masks list
 * 	str       pointer to the null-terminated file name
 *
 * Return value:
 * 	non-zero value if file name match masks, and zero if not
 */
int checkmasks(const char *masks, const char *str)
{
	int i = 0, not = 0;
	char *p = NULL;
	char *n = NULL;
	char *strs;
	int yield = 1;
	
	ASSERT(masks != NULL && str != NULL);
	
	strs = xstrcpy(masks);
	
	for( p = string_token(strs, &n, NULL, 1); p;
	     p = string_token(NULL, &n, NULL, 1) )
	{
		DEB((D_INFO, "checkmasks: checking \"%s\" in \"%s\"", str, p));
		
		not = ( p[0] == '!' );

		if( not )
			++p;
		
		if( p[0] == '%' )
		{
			for( i = 0; magfname[i].alias; i++ )
				if( strcasecmp(p+1, magfname[i].alias) == 0 )
				{
					if( !checkmasks(magfname[i].masks, str) )
					{
						free(strs);
						return not;
					}
					else if( not )
						yield = 0;
					
					break;
				}
		}
		else if( !strcasemask(str, p) )
		{
			free(strs);
			return not;
		}
		else if( not )
			yield = 0;
	}
	
	free(strs);
	return yield;
}

/*****************************************************************************
 * Put long integer (4 bytes) to the buffer
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	val       value that we want put to the buffer
 *
 * Return value:
 * 	Pointer to the specified buffer
 */
char *buffer_putlong(char *buffer, long val)
{
	buffer[0] = ( ((unsigned long) val)       ) & 0xff;
	buffer[1] = ( ((unsigned long) val) >> 8  ) & 0xff;
	buffer[2] = ( ((unsigned long) val) >> 16 ) & 0xff;
	buffer[3] = ( ((unsigned long) val) >> 24 ) & 0xff;
	
	return buffer;
}

/*****************************************************************************
 * Put integer (2 bytes) to the buffer
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	val       value that we want put to the buffer
 *
 * Return value:
 * 	Pointer to the specified buffer
 */
char *buffer_putint(char *buffer, int val)
{
	buffer[0] = ( ((unsigned int) val)       ) & 0xff;
	buffer[1] = ( ((unsigned int) val) >> 8  ) & 0xff;
	
	return buffer;
}

/*****************************************************************************
 * Get long integer (4 bytes) from the buffer
 *
 * Arguments:
 * 	buffer    pointer to the buffer
 *
 * Return value:
 * 	Extracted value
 */
long buffer_getlong(const char *buffer)
{
	return ( (unsigned long) ((unsigned char) buffer[0])       )
	     | ( (unsigned long) ((unsigned char) buffer[1]) << 8  )
	     | ( (unsigned long) ((unsigned char) buffer[2]) << 16 )
	     | ( (unsigned long) ((unsigned char) buffer[3]) << 24 );
}

/*****************************************************************************
 * Get integer (2 bytes) from the buffer
 *
 * Arguments:
 * 	buffer    pointer to the buffer
 *
 * Return value:
 * 	Extracted value
 */
int buffer_getint(const char *buffer)
{
	return ( (unsigned int) ((unsigned char) buffer[0])      )
	     | ( (unsigned int) ((unsigned char) buffer[1]) << 8 );
}

void printf_usage(const char *ident, const char *fmt, ...)
{
	va_list args;
	
	if( ident )
		printf("%s %s %s\n\n", BF_BANNERVER, ident, BF_COPYRIGHT);
	else
		printf("%s %s\n\n", BF_BANNERVER, BF_COPYRIGHT);
	
	if( fmt )
	{
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
	else
		printf("Sorry, no usage information available\n\n");
	
	printf("Mail bug reports to <adb@newmail.ru>\n");

	fflush(stdout);
}

/* end of u_misc.c */
