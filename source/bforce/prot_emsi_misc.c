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
#include "session.h"
#include "prot_emsi.h"

/* ------------------------------------------------------------------------- */
/* Add one char $ch to string $src.                                          */
/* Return pointer to reallocated string                                      */
/* ------------------------------------------------------------------------- */
static char *add_char(char *src, char ch)
{
	char *dest;
	int len;
	
	len = src ? strlen(src) : 0;
	dest = len ? xrealloc(src, len + 2) : xmalloc(2);
	*(dest + len    ) = ch;
	*(dest + len + 1) = '\0';
	
	return(dest);
}

/* ------------------------------------------------------------------------- */
/* Add string $add to string $source. Use escaping sequences for             */
/* ']' and '}' characters. Return pointer to reallocated string              */
/* ------------------------------------------------------------------------- */
static char *add_str(char *source, const char *add)
{
	char *dest;
	int len, pos;
	
	pos = strlen(source);
	len = pos + strlen(add) + 1;
	dest = (char *)xrealloc(source, len);

	while( *add )
	{
		if( (*add == ']') || (*add == '}') )
		{
			dest = (char *)xrealloc(dest, ++len);
			dest[pos++] = *add;
		} 

#ifdef USE_EMSI7BIT
		if( !isprint((unsigned char)*add) || (unsigned char)*add > '~' )
#else
		if( !isprint((unsigned char)*add) )
#endif
		{
			len += 2;
			dest = (char *)xrealloc(dest, len);
			sprintf(&dest[pos], "\\%02hd", *(unsigned char*)add);
			add += 1;
			pos += 3;
		}
		else
			dest[pos++] = *add++;
	}
	dest[pos] = '\0';
	
	return(dest);
}

/* ------------------------------------------------------------------------- */
/* Add EMSI-II flag to string $source. Return pointer to reallocated string  */
/* ------------------------------------------------------------------------- */
static char *add_nflag(char *source, const char *flag, int AKAn)
{
	unsigned char *dest;
	unsigned char buf[EMSI_MAXNFLAG+1];
	int len;
	
	sprintf(buf, "%s%d,", flag, AKAn);
	len = strlen(source) + strlen(buf) + 1;
	dest = (char *)xrealloc(source, len);
	strcat(dest, buf);
	
	return(dest);
}

/* ------------------------------------------------------------------------- */
/* Create emsi subpacket and return pointer to it                            */
/* ------------------------------------------------------------------------- */
char *emsi_createdat(s_emsi *emsi)
{
	int i;
	char *tmp = NULL;
	char buf[100];
	char abuf[BF_MAXADDRSTR+1];
	
	tmp = (char *)xstrcpy("**EMSI_DAT0000");
	
	/* {EMSI} indentifier
	 */
	if( emsi->have_emsi )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "EMSI");
		tmp = add_char(tmp, '}');
		
		/* our addresses, akas, etc :) */
		tmp = add_char(tmp, '{');
		for( i = 0; i < emsi->anum; i++ )
		{
			if( i ) tmp = add_char(tmp, ' ');
			tmp = add_str(tmp, ftn_addrstr(abuf, emsi->addrs[i].addr));
		}
		tmp = add_char(tmp, '}');
		
		/* password if avilable */
		tmp = add_char(tmp, '{');
		if( emsi->passwd[0] ) tmp = add_str(tmp, emsi->passwd);
		tmp = add_char(tmp, '}');
		
		/* link codes */
		tmp = add_char(tmp, '{');
		if( emsi->linkcodes.N81 ) tmp = add_str(tmp, "8N1,");
		if( emsi->linkcodes.PUP ) tmp = add_str(tmp, "PUP,");
		if( emsi->linkcodes.PUA ) tmp = add_str(tmp, "PUA,");
		if( emsi->linkcodes.NPU ) tmp = add_str(tmp, "NPU,");
		if( emsi->linkcodes.HAT ) tmp = add_str(tmp, "HAT,");
		if( emsi->linkcodes.PMO ) tmp = add_str(tmp, "PMO,");
		if( emsi->linkcodes.NFE ) tmp = add_str(tmp, "NFE,");
		if( emsi->linkcodes.NXP ) tmp = add_str(tmp, "NXP,");
		if( emsi->linkcodes.NRQ ) tmp = add_str(tmp, "NRQ,");
		if( emsi->linkcodes.HNM ) tmp = add_str(tmp, "HNM,");
		if( emsi->linkcodes.HXT ) tmp = add_str(tmp, "HXT,");
		if( emsi->linkcodes.HFE ) tmp = add_str(tmp, "HFE,");
		if( emsi->linkcodes.HRQ ) tmp = add_str(tmp, "HRQ,");
		if( emsi->linkcodes.FNC ) tmp = add_str(tmp, "FNC,");
		if( emsi->linkcodes.RMA ) tmp = add_str(tmp, "RMA,");
		if( emsi->linkcodes.RH1 ) tmp = add_str(tmp, "RH1,");
		for( i = 0; i < emsi->anum; i++ )
		{
			if( emsi->addrs[i].flags & EMSI_FLAG_PU )
				tmp = add_nflag(tmp, "PU", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_HA )
				tmp = add_nflag(tmp, "HA", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_PM )
				tmp = add_nflag(tmp, "PM", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_NF )
				tmp = add_nflag(tmp, "NF", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_NX )
				tmp = add_nflag(tmp, "NX", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_NR )
				tmp = add_nflag(tmp, "NR", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_HN )
				tmp = add_nflag(tmp, "HN", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_HX )
				tmp = add_nflag(tmp, "HX", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_HF )
				tmp = add_nflag(tmp, "HF", i);
			if( emsi->addrs[i].flags & EMSI_FLAG_HR )
				tmp = add_nflag(tmp, "HR", i);
		}
		if( tmp[strlen(tmp+1)] == ',' )
		{
			/* replace last ',' character */
			tmp[strlen(tmp+1)] = '}';
		}
		else
		{
			tmp = add_char(tmp, '}');
		}
			
		/* compatibility_codes */
		tmp = add_char(tmp, '{');
		if( emsi->compcodes.NCP ) tmp = add_str(tmp, "NCP,");
		if( emsi->compcodes.ZMO ) tmp = add_str(tmp, "ZMO,");
		if( emsi->compcodes.ZAP ) tmp = add_str(tmp, "ZAP,");
		if( emsi->compcodes.DZA ) tmp = add_str(tmp, "DZA,");
		if( emsi->compcodes.JAN ) tmp = add_str(tmp, "JAN,");
		if( emsi->compcodes.HYD ) tmp = add_str(tmp, "HYD,");
		if( emsi->compcodes.FRQ ) tmp = add_str(tmp, "FRQ,");
		if( emsi->compcodes.NRQ ) tmp = add_str(tmp, "NRQ,");
		if( emsi->compcodes.ARC ) tmp = add_str(tmp, "ARC,");
		if( emsi->compcodes.XMA ) tmp = add_str(tmp, "XMA,");
		if( emsi->compcodes.FNC ) tmp = add_str(tmp, "FNC,");
		if( emsi->compcodes.EII ) tmp = add_str(tmp, "EII,");
		if( emsi->compcodes.DFB ) tmp = add_str(tmp, "DFB,");
		if( emsi->compcodes.CHT ) tmp = add_str(tmp, "CHT,");
		if( emsi->compcodes.BBS ) tmp = add_str(tmp, "BBS,");
		if( emsi->compcodes.HFR ) tmp = add_str(tmp, "HFR,");
		if( tmp[strlen(tmp+1)] == ',' )
		{
			/* replace last ',' character */
			tmp[strlen(tmp+1)] = '}';
		}
		else
		{
			tmp = add_char(tmp, '}');
		}
		
		/* mailer product code */
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, emsi->m_pid[0]?emsi->m_pid:"fe");
		tmp = add_char(tmp, '}');
		
		/* mailer name */
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, emsi->m_name[0]?emsi->m_name:"BinkleyForce");
		tmp = add_char(tmp, '}');
		
		/* mailer version */
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, emsi->m_ver[0]?emsi->m_ver:"X.XXX");
		tmp = add_char(tmp, '}');
		
		/* mailer serial number */
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, emsi->m_reg[0]?emsi->m_reg:"Freeware");
		tmp = add_char(tmp, '}');
	}

	/* {IDENT} indentifier
	 */
	if( emsi->have_ident )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "IDENT");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		tmp = add_char(tmp, '[');
		if( emsi->sname[0] ) tmp = add_str(tmp, emsi->sname);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '[');
		if( emsi->location[0] ) tmp = add_str(tmp, emsi->location);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '[');
		if( emsi->sysop[0] ) tmp = add_str(tmp, emsi->sysop);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '[');
		if( emsi->phone[0] ) tmp = add_str(tmp, emsi->phone);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '[');
		if( emsi->speed )
		{
			sprintf(buf, "%d", emsi->speed);
			tmp = add_str(tmp, buf);
		}
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '[');
		if( emsi->flags[0] ) tmp = add_str(tmp, emsi->flags);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '}');
	}
	
	/* {TRAF} indentifier
	 */
	if( emsi->have_traf )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "TRAF");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		sprintf(buf, "%08lX %08lX",
			(long)emsi->netmail_size, (long)emsi->arcmail_size);
		tmp = add_str(tmp, buf);
		tmp = add_char(tmp, '}');
	}
	
	/* {MOH#} indentifier
	 */
	if( emsi->have_moh )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "MOH#");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		tmp = add_char(tmp, '[');
		sprintf(buf, "%08lX", (long)emsi->files_size);
		tmp = add_str(tmp, buf);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '}');
	}
	
	/* {OHFR} indentifier
	 */
	if( emsi->have_ohfr )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "OHFR");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		if( emsi->oh_time[0] )
		{
			tmp = add_str(tmp, emsi->oh_time);
			if( emsi->fr_time[0] )
			{
				/* send FR time only after OH time */
				tmp = add_char(tmp, ' ');
				tmp = add_str(tmp, emsi->fr_time);
			}
		}
		tmp = add_char(tmp, '}');
	}
	
	/* {TZUTC} indentifier
	 */
	if( emsi->have_tzutc )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "TZUTC");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		tmp = add_char(tmp, '[');
		sprintf(buf, "+%04d", emsi->tzutc);
		tmp = add_str(tmp, buf);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '}');
	}
	
	/* {XDATE} indentifier.. What's the fucking invention :-/
	 */
	if( emsi->have_xdatetime )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "XDATETIME");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		tmp = add_char(tmp, '[');
		tmp = add_str(tmp, emsi->xdatetime);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '}');
	}
	
	/* {TRX#} indentifier
	 */
	if( emsi->have_trx )
	{
		tmp = add_char(tmp, '{');
		tmp = add_str(tmp, "TRX#");
		tmp = add_char(tmp, '}');
		
		tmp = add_char(tmp, '{');
		tmp = add_char(tmp, '[');
		sprintf(buf, "%08lX", (long)emsi->time);
		tmp = add_str(tmp, buf);
		tmp = add_char(tmp, ']');
		tmp = add_char(tmp, '}');
	}
	
	/*
	 *  Write total length of <data_pkt>
	 */
	sprintf(buf, "%04hX", (short unsigned)strlen(tmp+14));
	memcpy(tmp+10, buf, 4);
	
	return(tmp);
}

/* ------------------------------------------------------------------------- */
/* Return substring between first pair of $from and $to characters           */
/* Warning: $str destructive                                                 */
/* ------------------------------------------------------------------------- */
static char *get_field(char **str, char from, char to)
{
	char *dst, *src, *dest = NULL;
	int ch;
	
	src = *str;
	
	if( *src++ != from )
		return NULL;
	
	dst = dest = src;
	
	while( *src )
	{
		if( *src == to )
		{
			if( *(src+1) == to )
				src++;
			else
				break;
		}
		else if( *src == '\\' )
		{
			if( *(src+1) == '\\' )
				src++;
			else
			{
				if( strspn(src+1, "0123456789abcdefABCDEF") == 2 )
				{
					sscanf(src+1, "%02x", &ch);
					*dst++ = (ch & 0xff);
					if( ch == ']' )
						*dst++ = ']';
					src += 3;
					continue;
				}
			}
		}
		*dst++ = *src++;
	}
	
	if( *src == '\0' )
		return NULL;
	
	*dst = '\0';
	*str = src + 1;
	
	return dest;
}

/* ------------------------------------------------------------------------- */
/* Check $p for EMSI-II link flag $name.. Such flags looks like XXn,         */
/* there n is an AKA number, on success return AKA number else (-1)          */
/* ------------------------------------------------------------------------- */
static int nflgcmp(const char *p, const char *name)
{
	int AKAn = -1;
	
	if( (strncmp(p, name, 2) == 0) &&
	    (strspn(p+2, "0123456789") == strlen(p+2)) )
	{
		AKAn = atoi(p+2);
	}
	
	return AKAn;
}

/* ------------------------------------------------------------------------- */
/* Parse emsi subpacket bracket by bracket. Return zero on success.          */
/* All obtained information put into $emsi structure                         */
/* ------------------------------------------------------------------------- */
int emsi_parsedat(char *emsi_dat, s_emsi *emsi)
{
	int i;
	char *tmp, *p, *q, *n;
	
	while( (tmp=get_field(&emsi_dat, '{', '}')) != NULL )
	{
		p = q = n = NULL;
		if( strcmp(tmp, "EMSI") == 0 )
		{
			emsi->have_emsi = 1;
			
			/* addresses */
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			for( p = string_token(p, &n, NULL, 0); p;
			     p = string_token(NULL, &n, NULL, 0) )
			{
				s_faddr addr;
				
				if( ftn_addrparse(&addr, p, FALSE) )
					log("unparsable address \"%s\" skipped", p);
				else
					session_addrs_add(&emsi->addrs, &emsi->anum, addr);
			}
			
			if( emsi->anum == 0 )
			{
				log("parsable addresses not found");
				return(1);
			}
			
			/* password */
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->passwd, p, sizeof(emsi->passwd));
					
			/* link codes */
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			DEB((D_HSHAKE, "parse_emsidat: link codes: %s", p));
			for( p = strtok(p, ","); p; p=strtok(NULL, ",") )
			{
				     if( !strcmp(p, "PUP") ) emsi->linkcodes.PUP = 1;
				else if( !strcmp(p, "PUA") ) emsi->linkcodes.PUA = 1;
				else if( !strcmp(p, "NPU") ) emsi->linkcodes.NPU = 1;
				else if( !strcmp(p, "HAT") ) emsi->linkcodes.HAT = 1;
				else if( !strcmp(p, "PMO") ) emsi->linkcodes.PMO = 1;
				else if( !strcmp(p, "NFE") ) emsi->linkcodes.NFE = 1;
				else if( !strcmp(p, "NXP") ) emsi->linkcodes.NXP = 1;
				else if( !strcmp(p, "NRQ") ) emsi->linkcodes.NRQ = 1;
				else if( !strcmp(p, "HNM") ) emsi->linkcodes.HNM = 1;
				else if( !strcmp(p, "HXT") ) emsi->linkcodes.HXT = 1;
				else if( !strcmp(p, "HFE") ) emsi->linkcodes.HFE = 1;
				else if( !strcmp(p, "HRQ") ) emsi->linkcodes.HRQ = 1;
				else if( !strcmp(p, "FNC") ) emsi->linkcodes.FNC = 1;
				else if( !strcmp(p, "RMA") ) emsi->linkcodes.RMA = 1;
				else if( !strcmp(p, "RH1") ) emsi->linkcodes.RH1 = 1;
				/* check EMSI-II address dependend flags */
				else if( ((i=nflgcmp(p, "HA")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_HA;
				else if( ((i=nflgcmp(p, "PM")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_PM;
				else if( ((i=nflgcmp(p, "NF")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_NF;
				else if( ((i=nflgcmp(p, "NX")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_NX;
				else if( ((i=nflgcmp(p, "NR")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_NR;
				else if( ((i=nflgcmp(p, "HN")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_HN;
				else if( ((i=nflgcmp(p, "HX")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_HX;
				else if( ((i=nflgcmp(p, "HF")) >= 0) && (i < emsi->anum) )
					emsi->addrs[i].flags |= EMSI_FLAG_HF;
				else if( ((i=nflgcmp(p, "HR")) >= 0) && (i < emsi->anum) ) 
					emsi->addrs[i].flags |= EMSI_FLAG_HR;
			}
			
			/* compatibility codes */
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			DEB((D_HSHAKE, "parse_emsidat: compatibility codes: %s", p));
			for( p = strtok(p, ","); p; p=strtok(NULL, ",") )
			{
				     if( !strcmp(p, "ZMO") ) emsi->compcodes.ZMO = 1;
				else if( !strcmp(p, "ZAP") ) emsi->compcodes.ZAP = 1;
				else if( !strcmp(p, "DZA") ) emsi->compcodes.DZA = 1;
				else if( !strcmp(p, "JAN") ) emsi->compcodes.JAN = 1;
				else if( !strcmp(p, "HYD") ) emsi->compcodes.HYD = 1;
				else if( !strcmp(p, "NCP") ) emsi->compcodes.NCP = 1;
				else if( !strcmp(p, "NRQ") ) emsi->compcodes.NRQ = 1;
				else if( !strcmp(p, "ARC") ) emsi->compcodes.ARC = 1;
				else if( !strcmp(p, "XMA") ) emsi->compcodes.XMA = 1;
				else if( !strcmp(p, "FNC") ) emsi->compcodes.FNC = 1;
				else if( !strcmp(p, "EII") ) emsi->compcodes.EII = 1;
				else if( !strcmp(p, "CHT") ) emsi->compcodes.CHT = 1;
				else if( !strcmp(p, "BBS") ) emsi->compcodes.BBS = 1;
				else if( !strcmp(p, "HFR") ) emsi->compcodes.HFR = 1;
			}
			
			/* mailer information */
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->m_pid, p, sizeof(emsi->m_pid));
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->m_name, p, sizeof(emsi->m_name));
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->m_ver, p, sizeof(emsi->m_ver));
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->m_reg, p, sizeof(emsi->m_reg));;
		}
		else if( strcmp(tmp, "IDENT") == 0 )
		{			
			emsi->have_ident = 1;
			
			if( (tmp=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->sname, p, sizeof(emsi->sname));
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->location, p, sizeof(emsi->location));
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->sysop, p, sizeof(emsi->sysop));
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->phone, p, sizeof(emsi->phone));;
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) sscanf(p, "%d", &emsi->speed);
			if( (p=get_field(&tmp, '[', ']')) == NULL ) return(1);
			if( p && *p ) strnxcpy(emsi->flags, p, sizeof(emsi->flags));
		}
		else if( strcmp(tmp, "TRX#") == 0 )
		{		
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( (p=get_field(&p, '[', ']'))        == NULL ) return(1);
			if( sscanf(p, "%08lx", &emsi->time) == 1 )
			{
				emsi->have_trx = 1;
			}
			else
			{
				log("Bad TRX# entry \"%s\"", p);
			}
		}
		else if( strcmp(tmp, "TRAF") == 0 )
		{	
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( p && *p )
			{
				if( sscanf(p, "%08X %08X", &emsi->netmail_size, &emsi->arcmail_size) == 2 )
				{
					emsi->have_traf = 1;
				}
				else
				{
					log("Bad TRAF entry \"%s\"", p);
				}
			}
		}
		else if( strcmp(tmp, "MOH#") == 0 )
		{
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( (p=get_field(&p, '[', ']')) == NULL ) return(1);
			if( p && *p )
			{
				if( sscanf(p, "%08X", &emsi->files_size) == 1 )
				{
					emsi->have_moh = 1;
				}
				else
				{
					log("Bad MOH# entry \"%s\"", p);
				}
			}
			else
			{
				log("Bad MOH# entry \"%s\"", p);
			}
		}
		else if( strcmp(tmp, "OHFR") == 0 )
		{
			emsi->have_ohfr = 1;
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			
			q = p;
			while( *q && !isspace(*q) ) q++;
			if( *q ) *q++ = '\0';
			if( *p ) strnxcpy(emsi->oh_time, p, sizeof(emsi->oh_time));
			
			while( *q &&  isspace(*q) ) q++;			
			if( *q ) strnxcpy(emsi->fr_time, q, sizeof(emsi->fr_time));
		}
		else if( strcmp(tmp, "XDATETIME") == 0 )
		{
			emsi->have_xdatetime = 1;
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( (p=get_field(&p, '[', ']')) == NULL ) return(1);
			if( *p ) strnxcpy(emsi->xdatetime, p, sizeof(emsi->xdatetime));
		}
		else if( strcmp(tmp, "TZUTC") == 0 )
		{
			emsi->have_tzutc = 1;
			
			if( (p=get_field(&emsi_dat, '{', '}')) == NULL ) return(1);
			if( (p=get_field(&p, '[', ']')) == NULL ) return(1);
		}
		else
		{
			strnxcat(emsi->addons, "{", sizeof(emsi->addons));
			strnxcat(emsi->addons, tmp, sizeof(emsi->addons));
			strnxcat(emsi->addons, "}", sizeof(emsi->addons));
		}
	} /* end of while */
	
	return(0);
}

void emsi_logdat(s_emsi *emsi)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	char buf[64];

	if( emsi->have_emsi )
	{
		if( emsi->anum )
		{
			for( i = 0; i < emsi->anum; i++ )
			{
				if( emsi->addrs[i].busy )
					log("   Address : %s (Busy)", ftn_addrstr(abuf, emsi->addrs[i].addr));
				else
					log("   Address : %s", ftn_addrstr(abuf, emsi->addrs[i].addr));
			}
		}
		else
		{
			log("   Address : <none>");
		}
	}

	if( emsi->have_ident )
	{
		if( emsi->sname[0] && emsi->phone[0] )
			log("    System : %s (%s)",
				string_printable(emsi->sname),
				string_printable(emsi->phone));
		else if( emsi->sname[0] )
			log("    System : %s",
				string_printable(emsi->sname));
		else if( emsi->phone[0] )
			log("     Phone : %s",
				string_printable(emsi->phone));
	}

	if( emsi->have_emsi )
	{
		char flags[128];
		*flags = '\0';
		
		if( emsi->linkcodes.PUP )
			strcat(flags, "PUP,");
		if( emsi->linkcodes.NPU )
			strcat(flags, "NPU,");
		if( emsi->linkcodes.HAT )
			strcat(flags, "HAT,");
		if( emsi->linkcodes.NRQ || emsi->compcodes.NRQ )
			strcat(flags, "NRQ,");
		if( emsi->linkcodes.HXT )
			strcat(flags, "HXT,");
		if( emsi->linkcodes.HRQ )
			strcat(flags, "HRQ,");
		if( emsi->linkcodes.FNC || emsi->compcodes.FNC )
			strcat(flags, "FNC,");
		if( emsi->compcodes.FRQ )
			strcat(flags, "FRQ,");
		if( emsi->compcodes.EII )
			strcat(flags, "EII,");
		if( emsi->compcodes.CHT )
			strcat(flags, "CHT,");
		if( emsi->compcodes.BBS )
			strcat(flags, "BBS,");
		if( emsi->compcodes.HFR )
			strcat(flags, "HFR,");

		if( *flags )
		{
			flags[strlen(flags)-1] = '\0';
			log("   Options : %s", flags);
		}
	}

#ifdef BFORCE_LOG_PASSWD
	if( emsi->have_emsi )
	{
		if( emsi->passwd[0] )
			log("  Password : %s", string_printable(emsi->passwd));
	}
#endif
	
	if( emsi->have_ident )
	{
		if( emsi->sysop[0] && emsi->location[0] )
			log("     SysOp : %s from %s",
				string_printable(emsi->sysop),
				string_printable(emsi->location));
		else if( emsi->sysop[0] )
			log("     SysOp : %s",
				string_printable(emsi->sysop));
		else if( emsi->location[0] )
			log("  Location : %s",
				string_printable(emsi->location));
		
		if( emsi->flags[0] && emsi->speed )
			log("     Flags : %d,%s",
				emsi->speed, string_printable(emsi->flags));
		else if( emsi->flags[0] )
			log("     Flags : %s",
				string_printable(emsi->flags));
		else if( emsi->speed )
			log("     Speed : %d", emsi->speed);
	}
	
	if( emsi->have_emsi )
	{
		if( emsi->m_name[0] || emsi->m_pid[0]
		 || emsi->m_ver[0]  || emsi->m_reg )
		{
			log("    Mailer : %s [%s] %s/%s",
				emsi->m_name[0] ? string_printable(emsi->m_name) : "?",
				emsi->m_pid[0]  ? string_printable(emsi->m_pid)  : "?",
				emsi->m_ver[0]  ? string_printable(emsi->m_ver)  : "?",
				emsi->m_reg[0]  ? string_printable(emsi->m_reg)  : "?");
		}
	}
	
	if( emsi->have_trx )
	{
		log("      Time : %s", time_string_long(buf, sizeof(buf), gmttolocal(emsi->time)));
	}

	if( emsi->have_xdatetime )
	{
		log(" XDATETIME : %s", string_printable(emsi->xdatetime));
	}

	if( emsi->have_ohfr )
	{
		log("      OHFR : \"%s\" and \"%s\"",
			string_printable(emsi->oh_time),
			string_printable(emsi->fr_time));
	}
	
	if( emsi->have_traf )
	{
		log("      TRAF : netmail %ldb, arcmail %ldb",
			emsi->netmail_size, emsi->arcmail_size);
	}

	if( emsi->have_moh )
	{
		log("      #MOH : files %ldb", emsi->files_size);
	}
	
	if( emsi->addons[0] )
	{
		log("EMSI addon : %s",
			string_printable(emsi->addons));
	}
}
