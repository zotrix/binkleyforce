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

#define ADDRNUM_MIN	0x0000
#define ADDRNUM_MAX	0x7fff

static int addrstr2int(const char *s)
{
	int val = atoi(s);
	
	if( val < ADDRNUM_MIN )
		val = ADDRNUM_MIN;
	else if( val > ADDRNUM_MAX )
		val = ADDRNUM_MAX;
	
	return val;
}

char *addrint2str(char *buffer, int val)
{
	if( val == -1 )
		strcpy(buffer, "*");
	else if( val < ADDRNUM_MIN )
		strcpy(buffer, "MIN");
	else if( val > ADDRNUM_MAX )
		strcpy(buffer, "MAX");
	else
		sprintf(buffer, "%d", val);
	
	return buffer;
}

static int ftn_addrparse_fido(s_faddr *addr, const char *s, bool wildcard)
{
	bool badaddr  = 0;
	bool stop     = 0;
	bool gotzone  = 0;
	bool gotnet   = 0;
	bool gotnode  = 0;
	bool gotpoint = 0;
	const char *p = s;

	ASSERT(s != NULL);

	while( !stop && !badaddr )
	{
		switch(*p) {
		case ':':
			if( !gotzone && isdigit(*s) )
			{
				gotzone = 1;
				addr->zone = addrstr2int(s);
				s = ++p;
			}
			else if( !gotzone && wildcard && *s == '*' )
			{
				gotzone = 1;
				addr->zone = -1;
				s = ++p;
			}
			else badaddr = 1;
			break;
			
		case '/':
			if( !gotnet && isdigit(*s) )
			{
				gotnet = 1;
				addr->net = addrstr2int(s);
				s = ++p;
			}
			else if( !gotnet && wildcard && *s == '*' )
			{
				gotnet = 1;
				addr->net = -1;
				s = ++p;
			}
			else if( *s != '/' ) badaddr = 1;
			else ++p;
			break;
			
		case '.':
			if( *s == '/' ) ++s;
			if( !gotnode && isdigit(*s) )
			{
				gotnode = 1;
				addr->node = addrstr2int(s);
				s = ++p;
			}
			else if( !gotnode && wildcard && *s == '*' )
			{
				gotnode = 1;
				addr->node = -1;
				s = ++p;
			}
			else if( *s != '.' ) badaddr = 1;
			else ++p;
			break;
			
		case '@':
		case '\0':
			if( gotzone && !gotnet ) { badaddr = 1; break; }
			
			if( *s == '/' )
			{
				++s;
				if( !gotnode && isdigit(*s) )
				{
					gotnode = 1;
					addr->node = addrstr2int(s);
				}
				else if( !gotnode && wildcard && *s == '*' )
				{
					gotnode = 1;
					addr->node = -1;
				}
				else badaddr = 1;
			}
			else if( *s == '.' )
			{
				++s;
				if( !gotpoint && isdigit(*s) )
				{
					gotpoint = 1;
					addr->point = addrstr2int(s);
				}
				else if( !gotpoint && wildcard && *s == '*' )
				{
					gotpoint = 1;
					addr->point = -1;
				}
				else badaddr = 1;
			}
			else if( isdigit(*s) )
			{
				if( !gotnode )
				{
					gotnode = 1;
					addr->node = addrstr2int(s);
				}
				else if( !gotpoint )
				{
					gotpoint = 1;
					addr->point = addrstr2int(s);
				}
				else badaddr = 1;
			}
			else if( wildcard && *s == '*' )
			{
				if( !gotnode )
				{
					gotnode = 1;
					addr->node = -1;
				}
				else if( !gotpoint )
				{
					gotpoint = 1;
					addr->point = -1;
				}
				else badaddr = 1;
			}
			else badaddr = 1;
			stop = 1;
			break;
			
		default:
			if( isdigit(*p) || (wildcard && *p == '*') )
				{ ++p; }
			else
				{ badaddr = 1; }
		}
	}
	
	if( !badaddr && *p++ == '@' && *p )
	{
		strnxcpy(addr->domain, string_printable(p), sizeof(addr->domain));
	}
	
	return badaddr;
}

static int ftn_addrparse_inet(s_faddr *addr, const char *s, bool wildcard)
{
	bool badaddr  = 0;
	bool stop     = 0;
	bool gotzone  = 0;
	bool gotnet   = 0;
	bool gotnode  = 0;
	bool gotpoint = 0;
	const char *p = s;

	ASSERT(s != NULL);

	while( !stop && !badaddr )
	{
		if( *p == '.' || *p == '\0' )
		{
			if( *p == '\0' ) stop = 1;
			
			switch(*s++) {
			case 'p':
			case 'P':
				if( !gotpoint && isdigit(*s) )
				{
					gotpoint = 1;
					addr->point = addrstr2int(s);
					s = ++p;
				}
				else badaddr = 1;
				break;
			case 'f':
			case 'F':
				if( !gotnet && !gotzone && !gotnode && isdigit(*s) )
				{
					gotnode = 1;
					addr->node = addrstr2int(s);
					s = ++p;
				}
				else badaddr = 1;
				break;
			case 'n':
			case 'N':
				if( gotnode && !gotzone && !gotnet && isdigit(*s) )
				{
					gotnet = 1;
					addr->net = addrstr2int(s);
					s = ++p;
				}
				else badaddr = 1;
				break;
			case 'z':
			case 'Z':
				if( gotnode && gotnet && !gotzone && isdigit(*s) )
				{
					gotzone = 1;
					addr->zone = addrstr2int(s);
					if( *p++ == '.' && *p )
					{
						strnxcpy(addr->domain, string_printable(p), sizeof(addr->domain));
					}
					stop = 1;
				}
				else badaddr = 1;
				break;
			default:
				badaddr = 1;
				break;
			}
		}
		else
		{
			++p;
		}
	}

	return badaddr;	
}

/*****************************************************************************
 * Parse FTN address (can be specified in traditional form (X:X/X.X) as
 * well as in domain form (pX.fX.nX.zX.domain))
 *
 * Arguments:
 * 	addr      we will put parsed address here
 * 	s         pointer to the null-terminated string with address
 * 	wildcard  set FALSE if you want forbid wildcards processing
 *
 * Return value:
 * 	zero value if addres was parsed successfuly, and non-zero if wasn't
 */
int ftn_addrparse(s_faddr *addr, const char *s, bool wildcard)
{
	const char *p;
	bool stop = 0;
	bool badaddr = 0;
	
	ASSERT(s != NULL);
	
	memset(addr, '\0', sizeof(s_faddr));

	/*
	 *  Detect the address form (domain or traditional)
	 */
	for( p = s; *p && !stop && !badaddr; p++ )
	{
		switch( *p ) {
		case '*':
			if( wildcard == FALSE ) badaddr = 1;
		case '@':
			stop = 1;
		case ':':
		case '/':
		case '.':
			addr->inetform = 0;
			break;
		case 'p':
		case 'P':
		case 'f':
		case 'F':
			addr->inetform = 1;
			stop = 1;
			break;
		default:
			if( !isdigit(*p) ) badaddr = 1;
		}
	}
	
	if( !badaddr )
	{
		addr->zone  = DEFAULT_ZONE;
		addr->net   = DEFAULT_NET;
		addr->node  = DEFAULT_NODE;
		addr->point = 0;

		if( addr->inetform )
			badaddr = ftn_addrparse_inet(addr, s, wildcard);
		else
			badaddr = ftn_addrparse_fido(addr, s, wildcard);
	}
	
	return badaddr;
}

char *ftn_addrstr_fido(char *buf, s_faddr addr)
{
	char str1[10];
	char str2[10];
	char str3[10];
	char str4[10];
	
	if( addr.point )
	{
		sprintf(buf, "%s:%s/%s.%s", 
			addrint2str(str1, addr.zone), addrint2str(str2, addr.net),
			addrint2str(str3, addr.node), addrint2str(str4, addr.point));
	}
	else
	{
		sprintf(buf, "%s:%s/%s", 
			addrint2str(str1, addr.zone), addrint2str(str2, addr.net),
			addrint2str(str3, addr.node));
	}

	if( *addr.domain )
	{
		strnxcat(buf, "@", BF_MAXADDRSTR);
		strnxcat(buf, addr.domain, BF_MAXADDRSTR);
	}

	return buf;
}

char *ftn_addrstr_inet(char *buf, s_faddr addr)
{
	char str1[10];
	char str2[10];
	char str3[10];
	char str4[10];
	
	if( addr.point )
	{
		sprintf(buf, "p%s.f%s.n%s.z%s", 
			addrint2str(str1, addr.point), addrint2str(str2, addr.node),
			addrint2str(str3, addr.net), addrint2str(str4, addr.zone));
	}
	else
	{
		sprintf(buf, "f%s.n%s.z%s", 
			addrint2str(str1, addr.node), addrint2str(str2, addr.net),
			addrint2str(str3, addr.zone));
	}

	if( *addr.domain )
	{
		strnxcat(buf, ".", BF_MAXADDRSTR);
		strnxcat(buf, addr.domain, BF_MAXADDRSTR);
	}

	return buf;
}

/*****************************************************************************
 * Put FTN address as the null-terminated string to the buffer
 *
 * Arguments:
 * 	buf       pointer to the destination buffer (must be at least
 * 	          BF_MAXADDRSTR bytes length)
 * 	addr      FTN address
 *
 * Return value:
 * 	pointer to the buffer start
 */
char *ftn_addrstr(char *buf, s_faddr addr)
{
	ASSERT(buf != NULL);
	
	return addr.inetform ? ftn_addrstr_inet(buf, addr)
	                     : ftn_addrstr_fido(buf, addr);
}

/*****************************************************************************
 * Compare two FTN addresses
 *
 * Arguments:
 * 	addr1     first FTN address
 * 	addr2     second FTN address
 *
 * Return value:
 * 	zero value if addresses are equal, and non-zero if not
 */
int ftn_addrcomp(s_faddr addr1, s_faddr addr2)
{
	return !( addr1.zone == addr2.zone && addr1.net == addr2.net
	       && addr1.node == addr2.node && addr1.point == addr2.point );
}

/*****************************************************************************
 * Compare two FTN addresses
 *
 * Arguments:
 * 	addr1     first FTN address
 * 	operator  logical operator (EQ|GT|LT)
 * 	addr2     second FTN address
 *
 * Return value:
 * 	zero value if expression is true, and non-zero if not
 */
int ftn_addrcomp_logic(s_faddr addr1, int operator, s_faddr addr2)
{
	int i;
	int matr1[4] = { addr1.zone, addr1.net, addr1.node, addr1.point };
	int matr2[4] = { addr2.zone, addr2.net, addr2.node, addr2.point };
	
	for( i = 0; i < 4; i++ )
	{
		switch(operator) {
		case ADDR_EQ:
			if( matr1[i] != matr2[i] )
				return 1;
			break;
		case ADDR_GT:
			if( matr1[i] > matr2[i] )
				return 0;
			else if( matr1[i] < matr2[i] )
				return 1;
			break;
		case ADDR_LT:
			if( matr1[i] < matr2[i] )
				return 0;
			else if( matr1[i] > matr2[i] )
				return 1;
			break;
		default:
			ASSERT(0);
		}
	}

	return 0;
}
/*****************************************************************************
 * Compare two FTN addresses (second address can contain wildcards)
 *
 * Arguments:
 * 	addr      first FTN address
 * 	mask      second FTN address (with wildcards)
 *
 * Return value:
 * 	zero value if addresses are equal, and non-zero if not
 */
int ftn_addrcomp_mask(s_faddr addr, s_faddr mask)
{
	return !( (addr.zone == mask.zone || mask.zone == -1)
	       && (addr.net == mask.net || mask.net == -1)
	       && (addr.node == mask.node  || mask.node == -1)
	       && (addr.point == mask.point || mask.point == -1) );
}

/*****************************************************************************
 * Get the metric of matching two addresses
 *
 * Arguments:
 * 	addr1     first FTN address
 * 	addr2     second FTN address
 *
 * Return value:
 * 	from 0 to 4 (0 - no matching, 4 - equal addresses)
 */
int ftn_addrsmetric(s_faddr addr1, s_faddr addr2)
{
	int metric = 0;
	
	if( addr1.zone == addr2.zone )
	{
		++metric;
		if( addr1.net == addr2.net )
		{
			++metric;
			if( addr1.node == addr2.node )
			{
				++metric;
				if( addr1.point == addr2.point )
				{
					++metric;
				}
			}
		}
	}
	
	return metric;
}

