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
#include "io.h"
#include "session.h"
#include "outbound.h"
#include "prot_common.h"
#include "prot_binkp.h"

/*****************************************************************************
 * Parse most common message arguments, send error message to remote
 * if needed.
 *
 * Arguments:
 * 	str       pointer to the string to parse
 * 	fn        pointer to the pointer for extracted file name
 * 	sz        pointer to the extracted file size
 * 	tm        pointer to the extracted file modification time
 * 	offs      pointer to the extracted file offset
 *
 * Return value:
 * 	non-zero value on error, zero on success
 */

int binkp_parsfinfo(const char *str, s_bpfinfo *fi, bool with_offset)
{
    int r;
    if( strlen(str)>PATH_MAX ) {
        log("too long string, overflow may occur");
        return -1;
    }
    r = sscanf(str, with_offset? "%s %d %d %d": "%s %d %d", &fi->fn, &fi->sz, &fi->tm, &fi->offs);
    if (r==(with_offset? 4: 3)) {
        return 0;
    }
    else {
        return -1;
    }
}

/*****************************************************************************
 * Send (queue) system information to the remote
 *
 * Arguments:
 * 	bpi       binkp state structure
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */
/*
void binkp_queue_sysinfo(s_bpinfo *bpi, s_binkp_sysinfo *binkp)
{
	int i;
	char *astr = NULL;
	char abuf[BF_MAXADDRSTR+1];
	
	if( !state.caller && binkp->challenge_length > 0 )
	{
		char challenge[128];
		string_bin_to_hex(challenge, binkp->challenge, binkp->challenge_length);
		binkp_queuemsgf(bpi, BPMSG_NUL, "OPT CRAM-MD5-%s", challenge);
	}	
	
	binkp_queuemsg(bpi, BPMSG_NUL, "SYS ", binkp->systname);
	binkp_queuemsg(bpi, BPMSG_NUL, "ZYZ ", binkp->sysop);
	binkp_queuemsg(bpi, BPMSG_NUL, "LOC ", binkp->location);
	binkp_queuemsg(bpi, BPMSG_NUL, "PHN ", binkp->phone);
	binkp_queuemsg(bpi, BPMSG_NUL, "NDL ", binkp->flags);

	binkp_queuemsg(bpi, BPMSG_NUL, "TIME ", binkp->timestr);
	
	binkp_queuemsgf(bpi, BPMSG_NUL, "VER %s %s/%d.%d",
		binkp->progname, binkp->protname,
		binkp->majorver, binkp->minorver);

	for( i = 0; i < binkp->anum; i++ )
	{
		if( i ) astr = xstrcat(astr, " ");
		astr = xstrcat(astr, ftn_addrstr(abuf, binkp->addrs[i].addr));
	}
	
	if( astr )
	{
		binkp_queuemsg(bpi, BPMSG_ADR, NULL, astr);
		free(astr);
	}
	if (state.caller)
	{
		char *szOpt = xstrcpy (" MB");
#ifndef NETSPOOL
		if (!nodelist_checkflag (state.node.flags, "NR"))
			szOpt = xstrcat (szOpt, " NR");
#endif
		if (!nodelist_checkflag (state.node.flags, "ND"))
			szOpt = xstrcat (szOpt, " ND");
		if (*szOpt)
			binkp_queuemsg(bpi, BPMSG_NUL, "OPT", szOpt);
		free (szOpt);
	}
}
*/

/*****************************************************************************
 * Write options to the log
 *
 * Arguments:
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */
void binkp_log_options(s_binkp_sysinfo *remote)
{
	if (remote->options & BINKP_OPT_MB) log ("We are in MB mode.");
	if (remote->options & BINKP_OPT_NR) log ("We are in NR mode.");
}

/*****************************************************************************
 * Write system information to the log
 *
 * Arguments:
 * 	binkp     structure with the system information
 *
 * Return value:
 * 	None
 */

void binkp_log_sysinfo(s_binkp_sysinfo *binkp)
{
	int i;
	char abuf[BF_MAXADDRSTR+1];
	
	if( binkp->anum )
		for( i = 0; i < binkp->anum; i++ )
		{
			log("   Address : %s", ftn_addrstr(abuf, binkp->addrs[i].addr));
		}
	
	if( *binkp->systname && *binkp->phone )
		log("    System : %s (%s)",
			string_printable(binkp->systname),
			string_printable(binkp->phone));
	else if( *binkp->systname )
		log("    System : %s",
			string_printable(binkp->systname));
	else if( *binkp->phone )
		log("     Phone : %s",
			string_printable(binkp->phone));

#ifdef BFORCE_LOG_PASSWD
	if( *binkp->passwd )
		log("  Password : %s", string_printable(binkp->passwd));
#endif

	if( *binkp->opt )
		log("   Options : %s", string_printable(binkp->opt));

	if( *binkp->sysop && *binkp->location )
		log("     SysOp : %s from %s",
			string_printable(binkp->sysop),
			string_printable(binkp->location));
	else if( *binkp->sysop )
		log("     SysOp : %s",
			string_printable(binkp->sysop));
	else if( *binkp->location )
		log("  Location : %s",
			string_printable(binkp->location));

	if( *binkp->progname )
	{
		log("    Mailer : %s (%s/%d.%d)",
			*binkp->progname ? string_printable(binkp->progname) : "?",
			*binkp->protname ? string_printable(binkp->protname) : "?",
			binkp->majorver, binkp->minorver);
	}
	if( *binkp->flags )
		log("     Flags : %s", string_printable(binkp->flags));
	if( *binkp->timestr )
		log("      Time : %s", string_printable(binkp->timestr));
}


/*****************************************************************************
 * Set our local system information 
 *
 * Arguments:
 * 	binkp       structure where to store system information
 * 	remote_addr remote main address (for setting best AKA)
 * 	caller      are we calling system?
 *
 * Return value:
 * 	None
 */
void binkp_set_sysinfo(s_binkp_sysinfo *binkp, s_faddr *remote_addr, bool caller)
{
	s_cval_entry *addr_ptr;
	s_cval_entry *hide_ptr;
	s_faddr *primary = NULL;
	
	const char *p_systname  = conf_string(cf_system_name);
	const char *p_location  = conf_string(cf_location);
	const char *p_sysop     = conf_string(cf_sysop_name);
	const char *p_phone     = conf_string(cf_phone);
	const char *p_flags     = conf_string(cf_flags);
	
	/* free previously allocated memory */
	if( binkp->addrs )
		free(binkp->addrs);

	memset(binkp, '\0', sizeof(s_binkp_sysinfo));
	
	/* Set best primary address */
	if( remote_addr )
	{
		primary = session_get_bestaka(*remote_addr);
		
		/* Add primary address */
		if( primary )
			session_addrs_add(&binkp->addrs, &binkp->anum, *primary);
	}

	/* Add other AKAs */
	for( addr_ptr = conf_first(cf_address); addr_ptr;
	     addr_ptr = conf_next(addr_ptr) )
	{
		for( hide_ptr = conf_first(cf_hide_our_aka); hide_ptr;
		     hide_ptr = conf_next(hide_ptr) )
		{
			if( !ftn_addrcomp(hide_ptr->d.falist.addr, addr_ptr->d.falist.addr) )
				break;
		}
		
		if( !hide_ptr && primary != &addr_ptr->d.falist.addr )
			session_addrs_add(&binkp->addrs, &binkp->anum,
					addr_ptr->d.falist.addr);
	}
	
	if( binkp->anum == 0 )
		log("warning: no addresses will be presented to remote");

	/* session password */
	if( caller )
		session_get_password(state.node.addr, binkp->passwd, sizeof(binkp->passwd));
	
	binkp->majorver = BINKP_MAJOR;
	binkp->minorver = BINKP_MINOR;
	
	strnxcpy(binkp->progname, BF_NAME"/"BF_VERSION"/"BF_OS, sizeof(binkp->progname));
	strnxcpy(binkp->protname, BINKP_NAME, sizeof(binkp->protname));
	
	strnxcpy(binkp->sysop,    p_sysop    ? p_sysop    : "Unknown", sizeof(binkp->sysop));
	strnxcpy(binkp->systname, p_systname ? p_systname : "Unknown", sizeof(binkp->systname));
	strnxcpy(binkp->location, p_location ? p_location : "Unknown", sizeof(binkp->location));
	strnxcpy(binkp->phone,    p_phone    ? p_phone    : "Unknown", sizeof(binkp->phone));
	strnxcpy(binkp->flags,    p_flags    ? p_flags    : "BINKP",   sizeof(binkp->flags));

	time_string_format(binkp->timestr, sizeof(binkp->timestr), "%Y/%m/%d %H:%M:%S", 0);
	
	/* Set session challenge string */
	if( !caller )
	{
		long rnd = (long)random();
		int  pid = ((int)getpid()) ^ ((int)random());
		long utm = (long)time(0);

		binkp->options |= BINKP_OPT_MD5;
		binkp->challenge[0] = (unsigned char)(rnd      );
		binkp->challenge[1] = (unsigned char)(rnd >> 8 );
		binkp->challenge[2] = (unsigned char)(rnd >> 16);
		binkp->challenge[3] = (unsigned char)(rnd >> 24);
		binkp->challenge[4] = (unsigned char)(pid      );
		binkp->challenge[5] = (unsigned char)(pid >> 8 );
		binkp->challenge[6] = (unsigned char)(utm      );
		binkp->challenge[7] = (unsigned char)(utm >> 8 );
		binkp->challenge[8] = (unsigned char)(utm >> 16);
		binkp->challenge[9] = (unsigned char)(utm >> 24);

		binkp->challenge_length = 10;
	}
}


void binkp_parse_options(s_binkp_sysinfo *binkp, char *options)
{
	char *p, *n;

	for( p = string_token(options, &n, NULL, 0); p;
	     p = string_token(NULL, &n, NULL, 0) )
	{
		if( !strcmp(p, "NR") ) {
			binkp->options |= BINKP_OPT_NR;
		} else
		if( !strcmp(p, "MB") )
			binkp->options |= BINKP_OPT_MB;
		else if( !strcmp(p, "MPWD") )
			binkp->options |= BINKP_OPT_MPWD;
		else if( !strncmp(p, "CRAM-", 5) )
		{
			char *hash_types = p + 5;
			char *challenge = strchr(hash_types, '-');

			if( challenge )
			{
				char *pp, *nn;
				
				*challenge++ = '\0';
				for( pp = string_token(hash_types, &nn, "/", 0); pp;
				     pp = string_token(NULL, &nn, "/", 0) )
				{
					if( !strcmp(pp, "SHA1") )
						binkp->options |= BINKP_OPT_SHA1;
					else if( !strcmp(pp, "MD5") )
						binkp->options |= BINKP_OPT_MD5;
					else if( !strcmp(pp, "DES") )
						binkp->options |= BINKP_OPT_DES;
				}
				
				if( strlen(challenge) > (2*sizeof(binkp->challenge)) )
					log("binkp got too long challenge string");
				else
					binkp->challenge_length = string_hex_to_bin(
							binkp->challenge, challenge);
			}
			else
				log("binkp got invalid option: \"%s\"", string_printable(p));
		}
	} 
}


