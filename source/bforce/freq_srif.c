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
#include "freq.h"
#include "session.h"

int req_createsrif(char *sname, char *req, char *rsp)
{
	FILE *fp;
	char *cptr;
	s_faddr *aptr;
	char abuf[BF_MAXADDRSTR+1];

	if( (fp = file_open(sname, "w")) == NULL )
	{
		logerr("can't create srif file \"%s\"", sname);
		return -1;
	}

	switch(state.session) {
	case SESSION_EMSI:
		fprintf(fp, "SessionType EMSI\n");
		break;
	case SESSION_BINKP:
		fprintf(fp, "SessionType OTHER\n");
		break;
	case SESSION_YOOHOO:
		fprintf(fp, "SessionType WAZOO\n");
		break;
	case SESSION_FTSC:
		fprintf(fp, "SessionType FTSC0001\n");
		break;
	case SESSION_UNKNOWN:
		ASSERT_MSG();
		break;
	}

	fprintf(fp, "Baud %ld\n",
		state.connspeed ? state.connspeed : 115200);
	
	fprintf(fp, "Time -1\n");
	fprintf(fp, "RequestList %s\n", req);
	fprintf(fp, "ResponseList %s\n", rsp);
	
	fprintf(fp, "RemoteStatus %s\n",
		state.protected ? "PROTECTED" : "UNPROTECTED");
	
	fprintf(fp, "SystemStatus %s\n",
		state.listed ? "LISTED" : "UNLISTED");
	
	if( state.handshake && state.handshake->remote_sysop_name
	 && (cptr = state.handshake->remote_sysop_name(state.handshake)) )
		fprintf(fp, "Sysop %s\n", cptr);
	else
		fprintf(fp, "Sysop SysOp\n");

	if( aptr = session_1remote_address() )
		fprintf(fp, "AKA %s\n", ftn_addrstr(abuf, *aptr));
	else
		return -1;
	
	if( state.handshake && state.handshake->remote_system_name
	 && (cptr = state.handshake->remote_system_name(state.handshake)) )
		fprintf(fp, "Site %s\n", cptr);
	
	if( state.handshake && state.handshake->remote_location
	 && (cptr = state.handshake->remote_location(state.handshake)) )
		fprintf(fp, "Location %s\n", cptr);
	
	if( state.handshake && state.handshake->remote_phone
	 && (cptr = state.handshake->remote_phone(state.handshake)) )
		fprintf(fp, "Phone %s\n", cptr);
	
	if( state.handshake && state.handshake->remote_mailer
	 && (cptr = state.handshake->remote_mailer(state.handshake)) )
		fprintf(fp, "Mailer %s\n", cptr);
	
	file_close(fp);

	return(0);
}

void req_addfilelist(char *listname, s_freq *freq)
{
	FILE *fp;
	char fnbuf[BF_MAXPATH+1], *p;
	int action = ACTION_NOTHING;
	struct stat st;
	
	if( (fp = file_open(listname, "r")) == NULL )
	{
		logerr("can't open freq answer list \"%s\" (%d)", listname, strlen(listname));
		return;
	}
	
	while( fgets(fnbuf, sizeof(fnbuf), fp) )
	{
		p      = fnbuf;
		action = ACTION_NOTHING;
		
		string_chomp(fnbuf);
		
		/*
		 * We won't remove leading and trailing spaces 
		 */
		
		if( *p == '\0' )
			continue; /* Empty line! */
		
		switch(*p) {
		case '=': ++p; action = ACTION_UNLINK; break;
		case '+': ++p; action = ACTION_NOTHING; break;
		case '-': ++p; action = ACTION_FORCEUNLINK; break;
		}
		
		if( stat(p, &st) == 0 )
		{
			DEB((D_FREQ, "adding file \"%s\", %d", p, st.st_size));
			
			(*freq->flast) = (s_filelist*)xmalloc(sizeof(s_filelist));
			memset(*freq->flast, '\0', sizeof(s_filelist));
			
			(*freq->flast)->fname  = xstrcpy(p);
			(*freq->flast)->size   = st.st_size;
			(*freq->flast)->action = action;
			(*freq->flast)->status = STATUS_WILLSEND;
			freq->flast = &(*freq->flast)->next;
			
			freq->fnumber += 1;
			freq->fsize   += st.st_size;
		}
		else
			logerr("can't stat file from answer list \"%s\"", p);
	}
	
	file_close(fp);
}

