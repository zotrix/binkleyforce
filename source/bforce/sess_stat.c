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
#include "bforce.h"
#include "util.h"
#include "nodelist.h"
#include "session.h"

/*
 * Format of 'sts' files (project only..)
 * 
 * Header:
 *  0  byte  - version (current is 1)
 *  1  byte  - global undialable flag (for all lines)
 *  2  word  - tries
 *  4  word  - handshake tries 
 *  6  dword - hold all calls until
 *  8  dword - node will not accept our file requests till this time
 *  10 dword - last successfull outgoing session
 *  12 dword - last successfull incoming session
 *
 * Each entry (per-line statistics)
 */

void session_stat_reset_counters(s_sess_stat *stat)
{
	stat->tries        = TRIES_RESET;
	stat->tries_noansw = TRIES_RESET;
	stat->tries_noconn = TRIES_RESET;
	stat->tries_nodial = TRIES_RESET;
	stat->tries_hshake = TRIES_RESET;
	stat->tries_sessns = TRIES_RESET;
}

static char *session_stat_get_stsfile(s_faddr *addr, int linenum)
{
	char  buf[32];
	char *yield = NULL;
	char *p_stsdir = conf_string(cf_status_directory);

	if( p_stsdir && *p_stsdir )
	{
		if( linenum == -1 )
			sprintf(buf, "%u.%u.%u.%u.sts",
					addr->zone, addr->net,
					addr->node, addr->point);
		else
			sprintf(buf, "%u.%u.%u.%u-%u.sts",
					addr->zone, addr->net,
					addr->node, addr->point, linenum);
		
		yield = string_concat(p_stsdir, buf, NULL);
	}
	
	return yield;
}

static int session_stat_read_stsfile(FILE *fp, s_sess_stat *stat)
{
	if( fseek(fp, 0, SEEK_SET) == -1 )
		return -1;
	
	memset(stat, '\0', sizeof(s_sess_stat));
	
	fscanf(fp, "%u %u %u %u %u %u %lu %lu %lu %lu",
			(unsigned int *) &stat->tries,
			(unsigned int *) &stat->tries_noconn,
			(unsigned int *) &stat->tries_noansw,
			(unsigned int *) &stat->tries_nodial,
			(unsigned int *) &stat->tries_hshake,
			(unsigned int *) &stat->tries_sessns,
			(unsigned long *) &stat->hold_until,
			(unsigned long *) &stat->hold_freqs,
			(unsigned long *) &stat->last_success_out,
			(unsigned long *) &stat->last_success_in);
	
	/* Set last successfull session time */
	stat->last_success = MAX(stat->last_success_out, stat->last_success_in);
	
	return 0;
}

static int session_stat_write_stsfile(FILE *fp, s_sess_stat *stat)
{
	if( fseek(fp, 0, SEEK_SET) == -1 )
		return -1;
	
	fprintf(fp, "%u %u %u %u %u %u %lu %lu %lu %lu",
			(unsigned int) stat->tries,
			(unsigned int) stat->tries_noconn,
			(unsigned int) stat->tries_noansw,
			(unsigned int) stat->tries_nodial,
			(unsigned int) stat->tries_hshake,
			(unsigned int) stat->tries_sessns,
			(unsigned long) stat->hold_until,
			(unsigned long) stat->hold_freqs,
			(unsigned long) stat->last_success_out,
			(unsigned long) stat->last_success_in);
	
	return 0;
}

static int session_stat_update_stsfile(const char *stsname, s_sess_stat *stat)
{
	int rc = 0;
	FILE *sts_fp = NULL;
	s_sess_stat tmpstat;
	bool stsexist = FALSE;
	
	memset(&tmpstat, '\0', sizeof(s_sess_stat));
	
	stsexist = (access(stsname, R_OK) == 0);
	
	if( (sts_fp = file_open(stsname, stsexist ? "r+" : "w")) == NULL )
	{
		logerr("cannot open status file \"%s\"", stsname);
		gotoexit(-1);
	}
	
	if( stsexist )
	{
		/* Ignore read errors */
		(void)session_stat_read_stsfile(sts_fp, &tmpstat);
		
		if( ftruncate(fileno(sts_fp), 0) == -1 )
		{
			logerr("cannot truncate status file \"%s\"", stsname);
			gotoexit(-1);
		}
	}
	
	/*
	 * Update statistic
	 */
	if( stat->tries == TRIES_RESET )
		tmpstat.tries = 0;
	else
		tmpstat.tries += stat->tries;
	
	if( stat->tries_noconn == TRIES_RESET )
		tmpstat.tries_noconn = 0;
	else
		tmpstat.tries_noconn += stat->tries_noconn;
	
	if( stat->tries_nodial == TRIES_RESET )
		tmpstat.tries_nodial = 0;
	else
		tmpstat.tries_nodial += stat->tries_nodial;

	if( stat->tries_noansw == TRIES_RESET )
		tmpstat.tries_noansw = 0;
	else
		tmpstat.tries_noansw += stat->tries_noansw;
	
	if( stat->tries_hshake == TRIES_RESET )
		tmpstat.tries_hshake = 0;
	else
		tmpstat.tries_hshake += stat->tries_hshake;
	
	if( stat->tries_sessns == TRIES_RESET )
		tmpstat.tries_sessns = 0;
	else
		tmpstat.tries_sessns += stat->tries_sessns;
	
	if( stat->hold_until == HOLD_RESET )
		tmpstat.hold_until = 0;
	else if( stat->hold_until > tmpstat.hold_until )
		tmpstat.hold_until = stat->hold_until;
	
	if( stat->hold_freqs == HOLD_RESET )
		tmpstat.hold_freqs = 0;
	else if( stat->hold_freqs > tmpstat.hold_freqs )
		tmpstat.hold_freqs = stat->hold_freqs;
	
	if( stat->last_success_out > tmpstat.last_success_out )
		tmpstat.last_success_out = stat->last_success_out;
	
	if( stat->last_success_in > tmpstat.last_success_in )
		tmpstat.last_success_in = stat->last_success_in;
	
	/*
	 * Write new statistic to the 'sts' file
	 */
	if( session_stat_write_stsfile(sts_fp, &tmpstat) == -1 )
	{
		logerr("error writing status file \"%s\"", stsname);
		gotoexit(-1);
	}
	
exit:
	if( sts_fp )
		file_close(sts_fp);
	
	return rc;
}

int session_stat_get(s_sess_stat *stat, s_faddr *addr)
{
	FILE *fp;
	char *stsname = session_stat_get_stsfile(addr, -1);

	memset(stat, '\0', sizeof(s_sess_stat));
	
	if( stsname )
	{
		fp = file_open(stsname, "r");
		if( !fp )
		{
			free(stsname);
			return -1;
		}
		
		(void)session_stat_read_stsfile(fp, stat);
		
		(void)file_close(fp);
		free(stsname);
	}
	
	return 0;
}

int session_stat_apply_diff(s_faddr addr, s_sess_stat stat)
{
	int rc;
	char *stsname = session_stat_get_stsfile(&addr, -1);
	
	if( !stsname )
		return -1;
	
	rc = session_stat_update_stsfile(stsname, &stat);
	
	free(stsname);
	return rc;
}

int session_stat_update(s_faddr *addr, s_sess_stat *stat, bool caller, int rc)
{
	char *stsname;
	
	/*
	 * Update sessions statistic depending on session
	 * return code. The latter must be one of BFERR
	 * values. Warning, stat structure might allready
	 * contain desired settings!
	 */
	if( rc == BFERR_NOERROR )
	{
		if( caller )
			stat->last_success_out = time(NULL);
		else
			stat->last_success_in = time(NULL);
		
		session_stat_reset_counters(stat);
	}
	else
	{
		if( caller )
		{
			stat->tries++;
			if( rc == BFERR_HANDSHAKE_ERROR )
				stat->tries_hshake++;
			if( rc == BFERR_CANT_CONNECT14 )
				stat->tries_noansw++;
			if( rc == BFERR_CANT_CONNECT13 )
				stat->tries_nodial++;
			if( BFERR_CANT_CONNECT10 <= rc && rc <= BFERR_CANT_CONNECT19 )
				stat->tries_noconn++;
			if( rc >= BFERR_CONNECT_TOOLOW )
				stat->tries_sessns++;
			
			/* Reset some counters */	
			if( rc >= BFERR_CONNECT_TOOLOW )
			{
				stat->tries_noansw = TRIES_RESET;
				stat->tries_noconn = TRIES_RESET;
			}
			if( rc >  BFERR_HANDSHAKE_ERROR )
				stat->tries_hshake = TRIES_RESET;
		}
	}
	
	stsname = session_stat_get_stsfile(addr, -1);
	if( !stsname )
		return -1;
	
	rc = session_stat_update_stsfile(stsname, stat);
	
	free(stsname);
	return rc;
}

