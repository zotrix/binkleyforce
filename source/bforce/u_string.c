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

#ifndef TEST
#include "includes.h"
#include "confread.h"
#include "logger.h"
#include "util.h"
#else
#include <stdio.h>
#include <stdarg.h>
#define TRUE     1
#define FALSE    0
#define bool     int
#define xmalloc  malloc
#define xrealloc realloc
#define ASSERT
#endif

/*
 * Maximum number of quoted-printable strings we will store together
 */
#define PRINTABLE_MAXITEMS 5

/*
 * Cyclic buffer for quoted-printable strings
 */
static char *printable_storage[PRINTABLE_MAXITEMS];

/*
 * Position of next entry to use in printable_storage[]
 */
static int printable_pos = -1;


/*****************************************************************************
 * Allocate memory for the copy of a string pointed by src
 *
 * Arguments:
 * 	src       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the resulting string (must be freed)
 */
char *xstrcpy(const char *src)
{
	char *tmp;

	if( !src )
		return NULL;
	
	tmp = xmalloc(strlen(src)+1);
	strcpy(tmp, src);
	
	return tmp;
}

/*****************************************************************************
 * Append string pointed by add to the string src, allocate memory for result
 *
 * Arguments:
 * 	src       pointer to the null-terminated string (will be freed)
 * 	add       this one will be appended to the src
 *
 * Return value:
 * 	Pointer to the resulting string (must be freed)
 */
char *xstrcat(char *src, const char *add)
{
	char *tmp;
	size_t size;

	if( !add || *add == '\0' )
		return src;
	
	size = (src ? strlen(src) : 0) + strlen(add);
	tmp  = (char*)xmalloc(size+1);
	
	if( src )
	{
		strcpy(tmp, src);
		free(src);
	} else
		*tmp = '\0';
	
	strcat(tmp, add);
	
	return tmp;
}

/*****************************************************************************
 * Copy a string to the buffer. Checks for the destination buffer overflow and
 * terminating '\0' character.
 *
 * Arguments:
 * 	dst       pointer to the destination buffer
 * 	src       string to copy from
 * 	len       destination buffer size
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *strnxcpy(char *dst, const char *src, size_t len)
{
	ASSERT(src != NULL && dst != NULL && len >= 0);
	
	dst[len - 1] = 0;
	
	return strncpy(dst, src, len - 1);
}

/*****************************************************************************
 * Add one string to the end of another. Checks for destination buffer
 * overflow and terminating '\0' character.
 *
 * Arguments:
 * 	dst       pointer to the destination buffer (append to the string
 * 	          stored in it)
 * 	src       append this string
 * 	len       destination buffer size
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *strnxcat(char *dst, const char *src, size_t len)
{
	int pos;
	
	ASSERT(src != NULL && dst != NULL && len >= 0);
	
	pos = strlen(dst);
	
	return strnxcpy(dst + pos, src, len - pos);
}

/*****************************************************************************
 * Get next substring from the string.
 *
 * Arguments:
 * 	str       pointer to a null-terminated string
 * 	next      points to the pointer where you want store address of
 * 	          the next substring for next calls
 * 	delim     delimiter characters, if NULL than the isspace() call used
 * 	          to check for the delimiter characters
 * 	quoted    ignore spaces in the quoted substrings
 *
 * Return value:
 * 	Pointer to the new substring or NULL if no more available
 */
char *string_token(char *str, char **next, const char *delim, int quoted)
{
	char *yield;
	char *p;
	
	ASSERT(next != NULL);
	
	yield = str ? str : *next;
	
	if( yield )
	{
		if( delim && *delim )
			while( *yield && strchr(delim, *yield) ) yield++;
		else
			while( isspace(*yield) ) yield++;
		
		if( *yield )
		{
			if( quoted && *yield == '"' )
			{
				p = ++yield;
				while( *p && *p != '"' ) p++;
			}
			else
			{
				p = yield;
				if( delim && *delim )
					while( *p && !strchr(delim, *p) ) p++;
				else
					while( *p && !isspace(*p) ) p++;
			}
			if( *p )
			{
				*p = '\0';
				*next = p+1;
			} else
				*next = NULL;
		} else
			yield = *next = NULL;
	}

	return yield;
}

/*****************************************************************************
 * Remove trailing CR and LF characters
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_chomp(char *str)
{
	char *p;

	ASSERT(str != NULL);
	
	if( str && *str )
	{
		p = str + strlen(str + 1);
		if( *p == '\n' )
			*p-- = '\0';
		if( *p == '\r' && p >= str )
			*p = '\0';
	}
	
	return str;
}

/*****************************************************************************
 * Find the firtst occurance of substring into the string
 *
 * Arguments:
 * 	substr    pointer to the null-terminated substring
 * 	string    pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the first occurrence of the substring in the string,
 * 	and NULL if substring is not found
 */
const char *string_casestr(const char *string, const char *substr)
{
	size_t subln = strlen(substr);
	size_t strln = strlen(string);
	
	while( *string && strln >= subln )
	{
		if( !strncasecmp(substr, string, subln) )
			return string;
		
		++string;
		--strln;
	}

	return NULL;
}

/*****************************************************************************
 * Find character in the string (It is like strchr(), but case insensitive)
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 * 	ch        character you want to find
 *
 * Return value:
 * 	Pointer to the first occurrence of the character in the string,
 * 	and NULL if character is not found
 */
const char *string_casechr(const char *str, int ch)
{
	ch = tolower(ch);
	
	while( *str )
	{
		if( tolower(*str) == ch )
			return str;

		++str;
	}

	return NULL;
}

/*****************************************************************************
 * Convert string to the upper case
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_toupper(char *str)
{
	char *p;
	
	ASSERT(str != NULL);
	
	for( p = str; *p; p++ ) *p = toupper(*p);
	
	return(str);
}

/*****************************************************************************
 * Convert string to the lower case
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_tolower(char *str)
{
	char *p;
	
	ASSERT(str != NULL);
	
	for( p = str; *p; p++ ) *p = tolower(*p);
	
	return(str);
}

/*****************************************************************************
 * Check wheather string doesn't contain lower case characters
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Return TRUE if string conforms our requirements, and FALSE if not
 */
bool string_isupper(const char *str)
{
	const char *p;
	
	ASSERT(str != NULL);
	
	for( p = str; *p; p++ )
	{
		if( isalpha(*p) && islower(*p) ) break;
	}

	return (*p == '\0') ? TRUE : FALSE;
}

/*****************************************************************************
 * Check wheather string doesn't contain upper case characters
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Return TRUE if string conforms our requirements, and FALSE if not
 */
bool string_islower(const char *str)
{
	const char *p;
	
	ASSERT(str != NULL);
	
	for( p = str; *p; p++ )
	{
		if( isalpha(*p) && isupper(*p) ) break;
	}
	
	return (*p == '\0') ? TRUE : FALSE;
}

/*****************************************************************************
 * Remove spaces at the end of string
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_trimright(char *str)
{
	char *p;

	ASSERT(str != NULL);

	if( str && *str )
	{
		p = str + strlen(str+1);
		while( p >= str && isspace(*p) ) *p-- = '\0';
	}
	
	return str;
}

/*****************************************************************************
 * Remove spaces at the begining of string
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	pointer to the string
 */
char *string_trimleft(char *str)
{
	char *p;
	
	ASSERT(str != NULL);

	if( str && *str )
	{
		p = str;
		while( isspace(*p) ) p++;
		if( p > str )
			memmove(str, p, strlen(p)+1);
	}
	
	return str;
}

/*****************************************************************************
 * Remove white spaces at the begining and at the end of string
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	pointer to the string
 */
char *string_trimboth(char *str)
{
	char *p;
	
	ASSERT(str != NULL);

	if( str && *str )
	{
		/* Remove leading spaces */
		p = str;
		while( isspace(*p) ) p++;
		if( p > str )
			memmove(str, p, strlen(p)+1);
		
		/* Remove trailing spaces */
		p = str + strlen(str+1);
		while( p >= str && isspace(*p) ) *p-- = '\0';
	}
	
	return str;
}

/*****************************************************************************
 * Quote all unprintable characters in the buffer (e.g. "\xff\xfe")
 *
 * Arguments:
 * 	buffer    pointer to the buffer to process
 * 	buflen    buffer size
 *
 * Return value:
 * 	Pointer to the quoted-printable string (must be freed)
 */
char *string_printable_buffer(const char *buffer, size_t buflen)
{
	char *dest, *p;
	const char *bp;
	int nonprintcount = 0;
	size_t pos = 0;
	
	for( bp = buffer, pos = 0; pos < buflen; bp++, pos++ )
	{
		if( !isprint((unsigned char)*bp) )
			nonprintcount++;
	}
	
	dest = xmalloc(buflen + nonprintcount * 4 + 1);
	
	if( nonprintcount == 0 )
	{
		memcpy(dest, buffer, buflen);
		dest[buflen] = '\0';
	}
	else
	{
		p = dest;
		for( pos = 0; pos < buflen; buffer++, pos++ )
		{
			if( !isprint((unsigned char)*buffer) )
			{
				sprintf(p, "\\x%02x", (unsigned char)*buffer);
				p += 4;
			} else
				*p++ = *buffer;
		}
		*p = '\0';
	}
	
	return dest;
}

/*****************************************************************************
 * Quote all unprintable characters in the string (e.g. "\xff\xfe")
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 *
 * Return value:
 * 	Pointer to the quoted-printable string. Memory allocated for the
 * 	resulting string will be freed automatically. So don't use more
 * 	than last PRINTABLE_MAXITEMS pointers returned by this function.
 */
const char *string_printable(const char *str)
{
	char *p;
	const char *q;
	int nonprintcount = 0;
	size_t pos = 0;
	size_t len = 0;
	
	if( printable_pos == -1 )
	{
		memset(printable_storage, '\0', sizeof(printable_storage));
		printable_pos = 0;
	}
	
	if( str == NULL ) return "(null)";

	len = strlen(str);
	
	for( q = str, pos = 0; pos < len; q++, pos++ )
	{
		if( iscntrl((unsigned char)*q) || !isprint((unsigned char)*q) )
			nonprintcount++;
	}
	
	if( !nonprintcount )
		return str;
	
	if( printable_pos >= PRINTABLE_MAXITEMS )
		printable_pos = 0;
	
	if( printable_storage[printable_pos] )
		free(printable_storage[printable_pos]);
	
	p = printable_storage[printable_pos] = xmalloc(len + nonprintcount * 4 + 1);
	
	for( pos = 0; pos < len; str++, pos++ )
	{
		if( iscntrl((unsigned char)*str) || !isprint((unsigned char)*str) )
		{
			sprintf(p, "\\x%02x", (unsigned char)*str);
			p += 4;
		} else
			*p++ = *str;
	}
	*p = '\0';
	
	return printable_storage[printable_pos++];
}

/*****************************************************************************
 * Replace all chracters 'oldchar' by the 'newchar'
 *
 * Arguments:
 * 	str       pointer to the null-terminated string
 * 	oldchar   old chracter
 * 	newchar   character to put istead of 'oldchar'
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_replchar(char *str, char oldchar, char newchar)
{
	char *p = str;
	
	while( *p )
	{
		if( *p == oldchar ) *p = newchar;
		++p;
	}
	
	return str;
}

/*****************************************************************************
 * Devide string on substrings and put pointers to them into the
 * dest[] array.
 *
 * Arguments:
 * 	dest      destination array where we should put substrings
 * 	items     number of entries in dest[]
 * 	str       string to parse (will be destroyed)
 * 	separator separator character
 *
 * Return value:
 * 	Number of substrings in the dest[]
 */
int string_parse(char **dest, int items, char *str, int separator)
{
	int count = 0;
	char *p = str;
	
	dest[count++] = str;
	
	while( *p && count < items )
	{
		if( *((unsigned char *)p) == separator )
		{
			*p++ = '\0';
			dest[count++] = p;
		} else
			++p;
	}

	return count;
}

/*****************************************************************************
 * Devide string on substrings separated by white-space characters and put
 * pointers to them into the dest[] array. Also it will ignore spaces in
 * quoted strings (quote characters will be removed)
 *
 * Arguments:
 * 	dest      destination array where we should put substrings
 * 	items     number of entries in dest[]
 * 	str       string to parse (will be destroyed)
 *
 * Return value:
 * 	Number of substrings in the dest[]
 */
int string_parse_regular(char **dest, int items, char *str)
{
	int count = 0;
	char *p = str;
	
	while( *p )
	{
		while( *p && isspace(*p) )
			++p;
		
		if( *((unsigned char *)p) == '"' && *++p )
		{
			dest[count++] = p;
			while( *p && *((unsigned char *)p) != '"' ) p++;
		}
		else if( *p )
		{
			dest[count++] = p;
			while( *p && !isspace(*p) ) p++;
		}
		
		if( *p && count < items )
			*p++ = '\0';
		else
			break;
	}

	return count;
}

/*****************************************************************************
 * Replace all substrings 'find' by the 'repl' in the string 'str'
 *
 * Arguments:
 * 	str       null-terminated string to process (will be unchanged)
 * 	find      substring to find
 * 	repl      replace substring
 *
 * Return value:
 * 	Pointer to the new string (must be freed)
 */
char *string_translate(const char *str, const char *find, const char *repl)
{
	size_t sz_find = strlen(find);
	size_t sz_repl = strlen(repl);
	size_t sz_dest = strlen(str);
	char *dest, *p;
	
	p = dest = xstrcpy(str);
	
	if( !sz_find ) return dest;
	
	while( *p )
	{
		if( memcmp(p, find, sz_find) == 0 )
		{
			size_t offset = p - dest;
			size_t newsize = sz_dest + (sz_repl - sz_find);
			
			if( newsize > sz_dest )
				dest = xrealloc(dest, newsize+1);
			
			if( sz_repl > sz_find )
				memmove(dest + offset + (sz_repl - sz_find), dest + offset, sz_dest - offset + 1);
			else if( sz_repl < sz_find )
				memmove(dest + offset, dest + offset + (sz_find - sz_repl), sz_dest - offset + 1);
			
			memcpy(dest + offset, repl, sz_repl);
			
			sz_dest = newsize;
			p = dest + offset + sz_repl;
		} else
			++p;
	}
	
	return dest;
}

/*****************************************************************************
 * Convert size into human readable format (e.g. 100,256Kb or 20.7Mb)
 *
 * Arguments:
 * 	buffer    pointer to the buffer for resulting string
 * 	size      size in bytes that you want to print
 *
 * Return value:
 * 	Pointer to the string
 */
char *string_humansize(char *buffer, size_t size)
{
	if( size < 1024 )
		sprintf(buffer, "%ldb", (long)size);
	else if( size < 100*1024 )
		sprintf(buffer, "%.1fK", ((double)size)/1024.0);
	else if( size < 1000*1024 )
		sprintf(buffer, "%ldK", (long)(size/1024));
	else if( size < 100*1024*1024 )
		sprintf(buffer, "%.1fM", ((double)size)/(1024.0*1024.0));
	else if( size < 1000*1024*1024 )
		sprintf(buffer, "%ldM", (long)(size/(1024*1024)));
	else
		sprintf(buffer, "%.1fG", ((double)size)/(1024.0*1024.0*1024.0));

	return string_replchar(buffer, ',', '.');
}

/*****************************************************************************
 * Interpret escape sequence (\xNN, \NNN, \r, \n, etc.)
 *
 * Arguments:
 * 	pptr      points a pointer to the first character after '\'
 *
 * Return value:
 * 	Value of the escaped character
 */
int string_get_escape(char **pptr)
{
	int ch = 0;
	static const char *hexdigits = "0123456789abcdef";
	char *p = *pptr;
	
	if( *p == 'x' )
	{
		const char *q;
		
		if( (q = string_casechr(hexdigits, *(++p))) )
		{
			ch = q - hexdigits;
			if( (q = string_casechr(hexdigits, *(++p))) )
			{
				ch = ch * 16 + (q - hexdigits);
			}
		}
	}
	else if( *p == '0' || *p == '1' || *p == '2' || *p == '3' )
	{
		ch = *(p++) - '0';
		if( isdigit(*p) && *p != '8' && *p != '9' )
		{
			ch = ch * 8 + (*(p++) - '0');
			if( isdigit(*p) && *p != '8' && *p != '9' )
			{
				ch = ch * 8 + (*p - '0');
			}
		}
	}
	else switch(*p) {
	case 'a':
		ch = '\a'; break;
	case 'b':
		ch = '\b'; break;
	case 'f':
		ch = '\f'; break;
	case 'n':
		ch = '\n'; break;
	case 'r':
		ch = '\r'; break;
	case 't':
		ch = '\t'; break;
	case '\\':
		ch = '\\'; break;
	default:
		ch = *p;
	}

	*pptr = p + 1;

	return (ch > 0) ? (ch & 0xff) : -1;
}

int string_dequote(char *dst, char *src)
{
	char *d = dst; 
	char *s = src;
	int ch;
	
	while( *s )
	{
		if( s[0] == '\\' && s[1] )
		{
			++s;
			ch = string_get_escape(&s);
			if( ch != -1 )
				*d++ = ch;
		}
		else
			*d++ = *s++;
	}

	*d = '\0';

	return 0;
}

char *string_concat(const char *str, ...)
{
	va_list args;
	size_t yield_len;
	char  *yield_ptr;
	char  *yield;
	char  *p;
	
	/*
	 * Calculate total length of yielding string
	 */
	yield_len = strlen(str);
	va_start(args, str);
	while( (p = va_arg(args, char *)) )
		yield_len += strlen(p);
	va_end(args);
	
	yield = xmalloc(yield_len + 1);
	strncpy(yield, str, yield_len);
	yield[yield_len] = '\0';
	yield_ptr = yield + strlen(yield);
	
	va_start(args, str);
	while( (p = va_arg(args, char *)) )
	{
		strcpy(yield_ptr, p);
		yield_ptr += strlen(p);
	}
	va_end(args);
	
	return yield;
}

void string_bin_to_hex(char *string, const char *binptr, int binlen)
{
	static const char *hexdigits = "0123456789abcdef";
	int i;

	for( i = 0; i < binlen; i++ )
	{
		*string++ = hexdigits[(*binptr >> 4) & 0x0f];
		*string++ = hexdigits[(*binptr     ) & 0x0f];
		++binptr;
	}
	*string = '\0';
}

int string_hex_to_bin(char *binptr, const char *string)
{
	static const char *hexdigits = "0123456789abcdef";
	int len = (int)strlen(string);
	int i, val;
	const char *p;
	char *dest = binptr;

	for( i = 0; 2*i < len; i++ )
	{
		if( (p = string_casechr(hexdigits, *(string++))) )
		{
			val = (int)(p - hexdigits);
			if( (p = string_casechr(hexdigits, *(string++))) )
			{
				val = val * 16 + (int)(p - hexdigits);
				*dest++ = (unsigned char)(val & 0xff);
			}
			else
				return 0;
		}
		else
			return 0;
	}

	return (int)(dest - binptr);
}

bool string_is_empty(const char *string)
{
	const char *p;

	for( p = string; *p; p++ )
		if( !isspace((int)(unsigned char)*p) )
			return FALSE;

	return TRUE;
}

#ifdef TEST
int main(void)
{
	char *hexstr = "0406ff2354124e6a9b2f6ed6ff00411287";
	char binbuf[64];
	int binlen;
	char newhex[64];
	
	/*
	 * string_hex_to_bin() and string_bin_to_hex()
	 */
	printf("*** Checking string_hex_to_bin() and string_bin_to_hex()\n");
	printf("Original string  : \"%s\"\n", hexstr);
	binlen = string_hex_to_bin(binbuf, hexstr);
	string_bin_to_hex(newhex, binbuf, binlen);
	printf("Resulting string : \"%s\"\n", newhex);
	if( strcmp(hexstr, newhex) )
		printf("<<< ERROR!!! >>>\n");
	
	return 0;
}
#endif /* TEST */

