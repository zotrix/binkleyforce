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

/*
 * Will call this function than add new entry to the queue
 */
int (*sysqueue_add_callback)(s_sysentry *) = NULL;

/*
 * Will call this function than remove entry from the queue
 */
int (*sysqueue_rem_callback)(s_sysentry *) = NULL;

/*
 * Set this to disable mail/files size count
 */
bool sysqueue_dont_count_sizes = FALSE;

/*
 * Return TRUE if 'chk' entry must be before 'max' in the queue
 */
static bool out_sysqueue_sort_comp(s_sysentry *max, s_sysentry *chk, int opts)
{
	if( opts & QUEUE_SORT_ADDRESS )
	{
		if( opts & QUEUE_SORT_REVERSE )
			return !ftn_addrcomp_logic(max->node.addr, ADDR_LT, chk->node.addr);
		else
			return !ftn_addrcomp_logic(max->node.addr, ADDR_GT, chk->node.addr);
	}
	else if( opts & QUEUE_SORT_SIZE )
	{
		size_t size_max = max->netmail_size + max->arcmail_size
		                + max->request_size + max->files_size;
		size_t size_chk = chk->netmail_size + chk->arcmail_size
		                + chk->request_size + chk->files_size;
		
		if( opts & QUEUE_SORT_REVERSE )
			return size_max > size_chk;
		else
			return size_max < size_chk;
	}

	return FALSE;
}

void out_sysqueue_sort(s_sysqueue *q, int opts)
{
	int i, j, max;
	s_sysentry tmp;
	
	if( q->sysnum < 2 ) return;
	
	for( i = 0; i < q->sysnum - 1; i++ )
	{
		for( max = i, j = i + 1; j < q->sysnum; j++ )
			if( out_sysqueue_sort_comp(&q->systab[max], &q->systab[j], opts)  )
				max = j;
		
		if( max > i )
		{
			tmp = q->systab[max];
			q->systab[max] = q->systab[i];
			q->systab[i] = tmp;
		}
	}		
}

void out_reset_sysqueue(s_sysqueue *q)
{
	int i;
	
	for( i = 0; i < q->sysnum; i++ )
	{
		q->systab[i].netmail_size = 0;
		q->systab[i].request_size = 0;
		q->systab[i].arcmail_size = 0;
		q->systab[i].files_size   = 0;
		q->systab[i].flavors      = 0;
		q->systab[i].types        = 0;
	}
}

void out_remove_from_sysqueue(s_sysqueue *q, int pos)
{
	ASSERT(q && pos >= 0 && pos < q->sysnum);
	
	if( sysqueue_rem_callback )
		(void)sysqueue_rem_callback(&q->systab[pos]);
	
	if( q->sysnum > 1 )
	{
		memmove(&q->systab[pos], &q->systab[pos + 1],
				sizeof(s_sysentry)*(q->sysnum - pos - 1));
		q->systab = xrealloc(q->systab,
				sizeof(s_sysentry)*(q->sysnum - 1));
		--q->sysnum;
	}
	else
	{
		free(q->systab);
		q->systab = NULL;
		q->sysnum = 0;
	}		
}

/*
 *  Return pointer to entry in systems list for address addr
 */
static s_sysentry *out_getsysentry(s_sysqueue *q, s_faddr addr)
{
	int i;
	
	for( i = 0; i < q->sysnum; i++ )
		if( ftn_addrcomp(q->systab[i].node.addr, addr) == 0 )
			return &q->systab[i];
	
	/* Add new entry for new address */

	q->systab = (s_sysentry*)xrealloc(q->systab, sizeof(s_sysentry)*(q->sysnum+1));

	memset(&q->systab[q->sysnum], '\0', sizeof(s_sysentry));
	q->systab[q->sysnum].node.addr = addr;

	if( sysqueue_add_callback )
		(void)sysqueue_add_callback(&q->systab[i]);
			
	return &q->systab[q->sysnum++];
}

/* ------------------------------------------------------------------------- */
/* Handle each file $fi found in outbound. Usually it called by outbound     */
/* scanner.. Pointer to result information will be stored at dst             */
/* ------------------------------------------------------------------------- */
int out_handle_sysqueue(s_outbound_callback_data *callback)
{
	s_sysentry *sentry;
	int type;
	int flavor;

	ASSERT(callback);
	ASSERT(callback->path);
	ASSERT(callback->dest);

	DEB((D_OUTBOUND, "out_handle_sysqueue: process file \"%s\"",
			callback->path));
	
	if( callback->type != OUTB_TYPE_FBOX )
	{
		int i;
		const char *p_ext = strrchr(callback->path, '.');

		if( !p_ext )
			return -1;
		
		for( i = 0; outtab[i].ext && strcasecmp(p_ext, outtab[i].ext); i++ );
		if( !outtab[i].ext )
			return -1;
		
		type = outtab[i].type;
		flavor = outtab[i].flavor;
	}
	else
	{
		type = TYPE_FILEBOX;
		if( callback->flavor != -1 )
			flavor = callback->flavor;
		else
			flavor = FLAVOR_HOLD;
	}
	
	/* Try to found existing entry for this address */
	if( (sentry = out_getsysentry((s_sysqueue*)callback->dest,
			callback->addr)) == NULL )
		return -1;
	
	if( type == TYPE_NETMAIL || type == TYPE_REQUEST || type == TYPE_FILEBOX )
	{
		struct stat st;
		
		sentry->flavors |= flavor;
		sentry->types   |= type;
		
		if( !sysqueue_dont_count_sizes )
		{
			if( stat(callback->path, &st) == 0 )
			{
				/* Update mail age */
				if( st.st_mtime > sentry->mailage )
					sentry->mailage = st.st_mtime;

				/* Update mail size */
				if( type & TYPE_NETMAIL )
					sentry->netmail_size += st.st_size;
				else if( type & TYPE_ARCMAIL )
					sentry->arcmail_size += st.st_size;
				else if( type & TYPE_REQUEST )
					sentry->request_size += st.st_size;
				else
					sentry->files_size += st.st_size;
			}
			else
				logerr("can't stat file \"%s\"", callback->path);
		}
	}
	else if( type == TYPE_FLOFILE )
	{
		sentry->flavors |= flavor;
		
		if( !sysqueue_dont_count_sizes )
		{
			s_FLO *FLO;
			
			if( (FLO = flo_open(callback->path, "r")) == NULL )
			{
				logerr("can't open flo \"%s\"", callback->path);
				return -1;
			}

			while( flo_next(FLO) == 0 )
			{
				struct stat st;

				if( stat(FLO->att_path, &st) == 0 )
				{
					/* Update mail age */
					if( st.st_mtime > sentry->mailage )
						sentry->mailage = st.st_mtime;
					
					/* Update mail size */
					if( out_filetype(FLO->att_path) & TYPE_ARCMAIL )
						sentry->arcmail_size += st.st_size;
					else
						sentry->files_size += st.st_size;
				}
			}
			flo_close(FLO);
		}
	}
	else
		log("skipping file of unknown type \"%s\"", callback->path);

	return 0;
}

void deinit_sysqueue(s_sysqueue *q)
{
	if( q->systab ) free(q->systab);
	memset(q, '\0', sizeof(s_sysqueue));
}

#ifdef DEBUG
void log_sysqueue(const s_sysqueue *q)
{
	char abuf[BF_MAXADDRSTR+1];
	char tmp[256] = "";
	int i;
	
	DEB((D_OUTBOUND, "log_sysqueue: BEGIN"));
	for( i = 0; i < q->sysnum; i++ )
	{
		DEB((D_OUTBOUND, "log_sysqueue: address %s", ftn_addrstr(abuf, q->systab[i].node.addr)));
		
		tmp[0] = '\0';
		
		if( q->systab[i].flavors & FLAVOR_IMMED  ) strcat(tmp, "Immediate,");
		if( q->systab[i].flavors & FLAVOR_CRASH  ) strcat(tmp, "Crash,");
		if( q->systab[i].flavors & FLAVOR_NORMAL ) strcat(tmp, "Normal,");
		if( q->systab[i].flavors & FLAVOR_HOLD   ) strcat(tmp, "Hold,");
		if( tmp[0] ) tmp[strlen(tmp)-1] = '\0';
		DEB((D_OUTBOUND, "log_sysqueue: \tflavors: \"%s\"", tmp));
		
		tmp[0] = '\0';
		if( q->systab[i].types & TYPE_NETMAIL  ) strcat(tmp, "netmail,");
		if( q->systab[i].types & TYPE_FLOFILE  ) strcat(tmp, "flofile,");
		if( q->systab[i].types & TYPE_ARCMAIL  ) strcat(tmp, "arcmail,");
		if( q->systab[i].types & TYPE_REQUEST  ) strcat(tmp, "request,");
		if( q->systab[i].types & TYPE_FILEECHO ) strcat(tmp, "fileecho,");
		if( q->systab[i].types & TYPE_FILEBOX  ) strcat(tmp, "filebox,");
		if( q->systab[i].types & TYPE_FROMFLO  ) strcat(tmp, "fromflo,");
		if( tmp[0] ) tmp[strlen(tmp)-1] = '\0';
		DEB((D_OUTBOUND, "log_sysqueue: \ttypes: \"%s\"", tmp));
	}
	DEB((D_OUTBOUND, "log_sysqueue: END"));
}
#endif
