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

typedef struct bsylist {
	s_faddr addr;
	char *name_4d;
	char *name_domain;
	char *name_amiga;
	struct bsylist *next;
#ifdef BFORCE_USE_CSY
	bool is_csy;
#endif
} s_bsylist;

s_bsylist *bsylist = NULL;

static int out_bsy_convert_csy_to_bsy(s_bsylist *ptrl);

/* ========================================================================= */
/* PRIVATE part                                                              */
/* ========================================================================= */

static int out_bsy_file_check(const char *name, const char *ext)
{
	char lockname[BFORCE_MAX_PATH+1];
	
	ASSERT(name != NULL && ext != NULL);
	
	strnxcpy(lockname, name, sizeof(lockname));
	strnxcat(lockname, ext, sizeof(lockname));
	
	return plock_check(lockname);
}

static int out_bsy_file_create(const char *name, const char *ext)
{
	char lockname[BFORCE_MAX_PATH+1];
	
	ASSERT(name != NULL && ext != NULL);
	
	strnxcpy(lockname, name, sizeof(lockname));
	strnxcat(lockname, ext, sizeof(lockname));
	
	return plock_create(lockname);
}

static int out_bsy_file_link(const char *oldname, const char *oldext,
                             const char *newname, const char *newext)
{
	char old_lockname[BFORCE_MAX_PATH+1];
	char new_lockname[BFORCE_MAX_PATH+1];
	
	ASSERT(oldname != NULL && oldext != NULL);
	ASSERT(newname != NULL && newext != NULL);
	
	strnxcpy(old_lockname, oldname, sizeof(old_lockname));
	strnxcat(old_lockname, oldext, sizeof(old_lockname));
	strnxcpy(new_lockname, newname, sizeof(new_lockname));
	strnxcat(new_lockname, newext, sizeof(new_lockname));
	
	return plock_link(new_lockname, old_lockname);
}

static int out_bsy_file_unlink(const char *name, const char *ext)
{
	char lockname[BFORCE_MAX_PATH+1];
	
	ASSERT(name != NULL && ext != NULL);
	
	strnxcpy(lockname, name, sizeof(lockname));
	strnxcat(lockname, ext, sizeof(lockname));
	
	return plock_remove(lockname);
}

/* ========================================================================= */
/* PUBLIC part                                                               */
/* ========================================================================= */

int out_bsy_check(s_faddr addr)
{
	bool isfailed = FALSE;
	char *name_4d = NULL;
	char *name_domain = NULL;
	char *name_amiga = NULL;

	name_4d     = out_getname_4d(addr);
	name_domain = out_getname_domain(addr);
	name_amiga  = out_getname_amiga(addr);
	
	DEB((D_OUTBOUND, "out_bsy_check: name_4d = \"%s\"", name_4d));
	DEB((D_OUTBOUND, "out_bsy_check: name_domain = \"%s\"", name_domain));
	DEB((D_OUTBOUND, "out_bsy_check: name_amiga = \"%s\"", name_amiga));
	
	/* Check for BSY files in all outbounds */
	if( (name_4d     && out_bsy_file_check(name_4d, ".bsy") == PLOCK_EXIST)
	 || (name_domain && out_bsy_file_check(name_domain, ".bsy") == PLOCK_EXIST)
	 || (name_amiga  && out_bsy_file_check(name_amiga, ".bsy") == PLOCK_EXIST) )
		isfailed = TRUE;
	
#ifdef BFORCE_USE_CSY
	if( !isfailed )
	{
		/* Check for CSY files in all outbounds */
		if( (name_4d     && out_bsy_file_check(name_4d, ".csy") == PLOCK_EXIST)
		 || (name_domain && out_bsy_file_check(name_domain, ".csy") == PLOCK_EXIST)
		 || (name_amiga  && out_bsy_file_check(name_amiga, ".csy") == PLOCK_EXIST) )
			isfailed = TRUE;
	}
#endif
	
	if( name_4d )
		free(name_4d);
	if( name_domain )
		free(name_domain);
	if( name_amiga )
		free(name_amiga);
	
	return isfailed ? -1 : 0;
}

#ifdef BFORCE_USE_CSY
int out_bsy_lock(s_faddr addr, bool csy_locks)
#else
int out_bsy_lock(s_faddr addr)
#endif
{
	char *name_4d = NULL;
	char *name_domain = NULL;
	char *name_amiga = NULL;
	bool locked_4d = FALSE;
	bool locked_domain = FALSE;
	bool locked_amiga = FALSE;
	s_bsylist bsytmp, **bsylst;
	bool isfailed = FALSE;
#ifdef DEBUG
	char abuf[BF_MAXADDRSTR+1];
#endif
#ifdef BFORCE_USE_CSY
	const char *lockext = csy_locks ? ".csy" : ".bsy";
#else
	const char *lockext = ".bsy";
#endif

	DEB((D_OUTBOUND, "out_bsy_lock: lock address %s",
		ftn_addrstr(abuf, addr)));

	/*
	 * Check, may be we've allready locked this address.
	 */
	for( bsylst = &bsylist; *bsylst; bsylst = &(*bsylst)->next )
	{
		if( !ftn_addrcomp((*bsylst)->addr, addr) )
		{
			DEB((D_OUTBOUND, "out_bsy_lock: address is allready locked"));
#ifdef BFORCE_USE_CSY
			if( (*bsylst)->is_csy && !csy_locks )
				return out_bsy_convert_csy_to_bsy(*bsylst);
#else
			return 0;
#endif
		}
	}
	
	name_4d     = out_getname_4d(addr);
	name_domain = out_getname_domain(addr);
	name_amiga  = out_getname_amiga(addr);

	DEB((D_OUTBOUND, "out_bsy_lock: name_4d = \"%s\"", name_4d));
	DEB((D_OUTBOUND, "out_bsy_lock: name_domain = \"%s\"", name_domain));
	DEB((D_OUTBOUND, "out_bsy_lock: name_amiga = \"%s\"", name_amiga));
	
	/* Check for BSY files in all outbounds */
	if( (name_4d     && out_bsy_file_check(name_4d, ".bsy") == PLOCK_EXIST)
	 || (name_domain && out_bsy_file_check(name_domain, ".bsy") == PLOCK_EXIST)
	 || (name_amiga  && out_bsy_file_check(name_amiga, ".bsy") == PLOCK_EXIST) )
		isfailed = TRUE;
	
#ifdef BFORCE_USE_CSY
	if( !isfailed )
	{
		/* Check for CSY files in all outbounds */
		if( (name_4d     && out_bsy_file_check(name_4d, ".csy") == PLOCK_EXIST)
		 || (name_domain && out_bsy_file_check(name_domain, ".csy") == PLOCK_EXIST)
		 || (name_amiga  && out_bsy_file_check(name_amiga, ".csy") == PLOCK_EXIST) )
			isfailed = TRUE;
	}
#endif
	
	DEB((D_OUTBOUND, "out_bsy_lock: lock check result: %s",
		isfailed ? "Locked" : "Not locked"));
	
	/* Create ?SY file in 4D outbound */
	if( !isfailed && name_4d )
	{
		switch( out_bsy_file_create(name_4d, lockext) ) {
		case PLOCK_OK:	locked_4d = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_4d = FALSE; break;
		}
	}
	
	/* Create ?SY file in domain outbound */
	if( !isfailed && name_domain )
	{
		switch( out_bsy_file_create(name_domain, lockext) ) {
		case PLOCK_OK:	locked_domain = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_domain = FALSE; break;
		}
	}

	/* Create ?SY file in "AmigaDos" outbound */
	if( !isfailed && name_amiga )
	{
		switch( out_bsy_file_create(name_amiga, lockext) ) {
		case PLOCK_OK:	locked_amiga = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_amiga = FALSE; break;
		}
	}

#ifdef BFORCE_USE_CSY
	if( !isfailed )
	{
		/* Check for BSY files in all outbounds */
		if( (name_4d     && out_bsy_file_check(name_4d, ".bsy") == PLOCK_EXIST)
		 || (name_domain && out_bsy_file_check(name_domain, ".bsy") == PLOCK_EXIST)
		 || (name_amiga  && out_bsy_file_check(name_amiga, ".bsy") == PLOCK_EXIST) )
			isfailed = TRUE;
	}
#endif
	
	DEB((D_OUTBOUND, "out_bsy_lock: lock create result: %s",
		isfailed ? "Failed" : "Successful"));

	/* Somebody else create BSY before us?! */
	if( isfailed )
	{
		if( locked_4d )
			out_bsy_file_unlink(name_4d, lockext);
		if( locked_domain )
			out_bsy_file_unlink(name_domain, lockext);
		if( locked_amiga )
			out_bsy_file_unlink(name_amiga, lockext);
		
		if( name_4d )
			free(name_4d);
		if( name_domain )
			free(name_domain);
		if( name_amiga )
			free(name_amiga);

		return -1;
	}
	
	/*
	 * Successful locking. Add new entry to the ``bsylist''.
	 */
	
	memset(&bsytmp, '\0', sizeof(s_bsylist));
	
	bsytmp.addr = addr;
	
#ifdef BFORCE_USE_CSY
	bsytmp.is_csy = csy_locks;
#endif
	
	if( locked_4d )
		bsytmp.name_4d = name_4d;
	if( locked_domain )
		bsytmp.name_domain = name_domain;
	if( locked_amiga )
		bsytmp.name_amiga = name_amiga;
	
	for( bsylst = &bsylist; *bsylst; bsylst = &(*bsylst)->next )
	{
		/* EMPTY LOOP */
	}
	
	(*bsylst) = (s_bsylist*)xmalloc(sizeof(s_bsylist));
	**bsylst = bsytmp;
	
	return 0;
}

int out_bsy_unlockall(void)
{
	s_bsylist *ptrl, *next;
#ifdef DEBUG
	char abuf[BFORCE_MAX_ADDRSTR+1];
#endif
	
	for( ptrl = bsylist; ptrl; ptrl = next )
	{
#ifdef BFORCE_USE_CSY
		const char *lockext = ptrl->is_csy ? ".csy" : ".bsy";
#else
		const char *lockext = ".bsy";
#endif
		next = ptrl->next;
		
		DEB((D_OUTBOUND, "out_bsy_unlockall: unlock address %s",
			ftn_addrstr(abuf, ptrl->addr)));
		
		if( ptrl->name_4d )
		{
			out_bsy_file_unlink(ptrl->name_4d, lockext);
			free(ptrl->name_4d);
		}
		
		if( ptrl->name_domain )
		{
			out_bsy_file_unlink(ptrl->name_domain, lockext);
			free(ptrl->name_domain);
		}
		
		if( ptrl->name_amiga )
		{
			out_bsy_file_unlink(ptrl->name_amiga, lockext);
			free(ptrl->name_amiga);
		}
		
		free(ptrl);
	}
	
	bsylist = NULL;
	
	return 0;
}

#ifdef BFORCE_USE_CSY
static int out_bsy_convert_csy_to_bsy(s_bsylist *ptrl)
{
	bool locked_4d = FALSE;
	bool locked_domain = FALSE;
	bool locked_amiga = FALSE;
	bool isfailed = FALSE;
#ifdef DEBUG
	char abuf[BF_MAXADDRSTR+1];
#endif
	
	DEB((D_OUTBOUND, "out_bsy_convert_csy_to_bsy: process address %s",
		ftn_addrstr(abuf, ptrl->addr)));

	if( !ptrl->is_csy )
	{
		DEB((D_OUTBOUND, "out_bsy_convert_csy_to_bsy: skip (no csy locks)"));
		return 0;
	}

#define LINK_CSY_TO_BSY(lockname) \
	out_bsy_file_link(lockname, ".csy", lockname, ".bsy")
	
	/* Create BSY file in 4D outbound */
	if( !isfailed && ptrl->name_4d )
	{
		switch( LINK_CSY_TO_BSY(ptrl->name_4d) ) {
		case PLOCK_OK:	locked_4d = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_4d = FALSE; break;
		}
	}
	
	/* Create BSY file in domain outbound */
	if( !isfailed && ptrl->name_domain )
	{
		switch( LINK_CSY_TO_BSY(ptrl->name_domain) ) {
		case PLOCK_OK:	locked_domain = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_domain = FALSE; break;
		}
	}

	/* Create BSY file in "AmigaDos" outbound */
	if( !isfailed && ptrl->name_amiga )
	{
		switch( LINK_CSY_TO_BSY(ptrl->name_amiga) ) {
		case PLOCK_OK:	locked_amiga = TRUE; break;
		case PLOCK_EXIST:	isfailed = TRUE; break;
		case PLOCK_ERROR:	locked_amiga = FALSE; break;
		}
	}

	DEB((D_OUTBOUND, "out_csy_to_bsy: lock create result: %s",
		isfailed ? "Failed" : "Successful"));

	/* Somebody else create BSY before us?! */
	if( isfailed )
	{
		if( locked_4d )
			out_bsy_file_unlink(ptrl->name_4d, ".bsy");
		if( locked_domain )
			out_bsy_file_unlink(ptrl->name_domain, ".bsy");
		if( locked_amiga )
			out_bsy_file_unlink(ptrl->name_amiga, ".bsy");
		
		/* Leave ``.csy'' files untouched! */
		
		return 1;
	}

	ptrl->is_csy = FALSE;
	
	return 0;
}
#endif /* BFORCE_USE_CSY */

