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

int out_filetype(const char *fname)
{
	const char *p_nam;
	const char *p_ext;
	int i;
	
	/*
	 *  Get only file name, w/o dir part
	 */
	if( (p_nam = strrchr(fname, DIRSEPCHR)) )
		p_nam++;
	else
		p_nam = fname;
	
	/*
	 *  Get file name extension
	 */
	if( (p_ext = strrchr(p_nam, '.')) == NULL )
		return TYPE_UNKNOWN;
	
	for( i = 0; outtab[i].ext; i++ )
		if( strcasemask(p_ext, outtab[i].ext) == 0 )
			return outtab[i].type;
	
	for( i = 0; exttab[i].ext; i++ )
		if( strcasemask(p_ext, exttab[i].ext) == 0 )
			return exttab[i].type;
	
	return TYPE_UNKNOWN;
}

/* ------------------------------------------------------------------------- */
/* Read FLO file line by line and call handler proc for each file in FLO     */
/* ------------------------------------------------------------------------- */
static int out_readflo(s_filelist **fslist, const char *floname,
		int flavor, bool aso, int flodsc)
{
	s_FLO *FLO;
	s_filelist **tmpl;
	struct stat st;
	int type;
    
	DEB((D_OUTBOUND, "out_readflo: opening flo file \"%s\"", floname));
	
	if( (FLO = flo_open(floname, "r")) == NULL )
	{
		logerr("can't open flo \"%s\"", floname);
		return -1;
	}
    
	for( tmpl = fslist; *tmpl; tmpl = &(*tmpl)->next );

	while( flo_next(FLO) == 0 )
	{
		if( stat(FLO->att_path, &st) == 0 )
		{
			type  = out_filetype(FLO->att_path);
			type |= TYPE_FROMFLO;
			type |= aso ? TYPE_ASONAME : 0;
	
			(*tmpl) = (s_filelist*)xmalloc(sizeof(s_filelist));
			memset(*tmpl, '\0', sizeof(s_filelist));
			(*tmpl)->fname   = (char *)xstrcpy(FLO->att_path);
			(*tmpl)->size    = st.st_size;
			(*tmpl)->type    = type;
			(*tmpl)->flavor  = flavor;
			(*tmpl)->action  = FLO->att_action;
			(*tmpl)->status  = STATUS_WILLSEND;
			(*tmpl)->flodsc  = flodsc;
			tmpl = &(*tmpl)->next;
		}
		else
			logerr("can't stat file \"%s\" from flo \"%s\"",
					FLO->att_path, floname);
	}
	
	flo_close(FLO);
	return 0;
}

/*
 *  Add new entry to the flo files table, return it's index number
 */
static int out_addfloentry(s_fsqueue *q, const char *floname, int flavor)
{
	q->flotab = (s_flofile*)xrealloc(q->flotab, sizeof(s_flofile)*(q->flonum+1));

	memset(&q->flotab[q->flonum], '\0', sizeof(s_flofile));
	q->flotab[q->flonum].fname  = xstrcpy(floname);
	q->flotab[q->flonum].flavor = flavor;
	q->flonum++;

	return q->flonum-1;
}

/* ------------------------------------------------------------------------- */
/* Handle each file $fi found in outbound. Usually it called by outbound     */
/* scanner.. Pointer to result information will be stored at dst             */
/* ------------------------------------------------------------------------- */
int out_handle_fsqueue(s_outbound_callback_data *callback)
{
	s_fsqueue *queue;
	int type;
	int flavor;
	int action;
	
	ASSERT(callback);
	ASSERT(callback->path);
	ASSERT(callback->dest);

	queue = (s_fsqueue*)callback->dest;
	
	DEB((D_OUTBOUND, "out_handle_fsqueue: process file \"%s\"",
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
		action = outtab[i].action;
	}
	else
	{
		type = out_filetype(callback->path) | TYPE_FILEBOX;
		if( callback->flavor != -1 )
			flavor = callback->flavor;
		else
			flavor = FLAVOR_HOLD;
		action = ACTION_UNLINK;
	}
	
	if( (type & TYPE_NETMAIL) == TYPE_NETMAIL
	 || (type & TYPE_REQUEST) == TYPE_REQUEST
	 || (type & TYPE_FILEBOX) == TYPE_FILEBOX )
	{
		struct stat st;
		if( stat(callback->path, &st) == 0 )
		{
			s_filelist **tmpl;
			for( tmpl = &queue->fslist; *tmpl; tmpl = &(*tmpl)->next );
			(*tmpl) = (s_filelist*)xmalloc(sizeof(s_filelist));
			memset(*tmpl, '\0', sizeof(s_filelist));
			(*tmpl)->fname   = xstrcpy(callback->path);
			(*tmpl)->size    = st.st_size;
			(*tmpl)->type    = type |
					(callback->type == OUTB_TYPE_ASO ? TYPE_ASONAME : 0);
			(*tmpl)->flavor  = flavor;
			(*tmpl)->action  = action;
			(*tmpl)->status  = STATUS_WILLSEND;
			(*tmpl)->flodsc  = -1;
		}
		else
			logerr("can't stat file \"%s\"", callback->path);
	}
	else if( (type & TYPE_FLOFILE) == TYPE_FLOFILE )
	{
		out_readflo(&queue->fslist, callback->path,
				flavor, (callback->type == OUTB_TYPE_ASO),
				out_addfloentry(queue, callback->path, flavor));
	}

	return 0;
}

void deinit_filelist(s_filelist *filelist)
{
	struct filelist *ptrl, *next;
	
	for( ptrl = filelist; ptrl; ptrl = next )
	{
		next = ptrl->next;
		if( ptrl->fname ) free(ptrl->fname);
		free(ptrl);
	}
}

void deinit_fsqueue(s_fsqueue *q)
{
	int i;

	if( q->fslist )
		deinit_filelist(q->fslist);

	if( q->flotab )
	{
		for( i = 0; i < q->flonum; i++ )
			if( q->flotab[i].fname ) free(q->flotab[i].fname);
		free(q->flotab);
	}
	
	memset(q, '\0', sizeof(s_fsqueue));
}

#ifdef DEBUG
void log_filelist(const s_filelist *filelist)
{
	const s_filelist *ptrl;
	
	DEB((D_OUTBOUND, "log_filelist: BEGIN"));
	for( ptrl = filelist; ptrl; ptrl = ptrl->next )
	{
		DEB((D_OUTBOUND, "log_filelist: \tfile \"%s\", %ld byte(s), flodsc = %d, status: \"%s\"",
		     ptrl->fname, (long)ptrl->size, ptrl->flodsc,
		     ptrl->status == STATUS_WILLSEND ? "WILLSEND" :
		     ptrl->status == STATUS_SKIP     ? "SKIP"     :
		     ptrl->status == STATUS_SKIP     ? "SENDING"  :
		     ptrl->status == STATUS_SENT     ? "SENT"     : "?"));
	}
	DEB((D_OUTBOUND, "log_filelist: END"));
}

void log_fsqueue(const s_fsqueue *q)
{
	int i;
	
	DEB((D_OUTBOUND, "log_queue_long: BEGIN"));

	log_filelist(q->fslist);

	for( i = 0; i < q->flonum; i++ )
		DEB((D_OUTBOUND, "log_queue_long: FLO file \"%s\"", q->flotab[i].fname));

	DEB((D_OUTBOUND, "log_queue_long: END"));
}

#endif
