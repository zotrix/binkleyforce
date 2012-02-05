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
#include "io.h"
#include "util.h"
#include "session.h"
#include "prot_common.h"
#include "outbound.h"
#include "freq.h"

const char *Protocols[] =
{
	"Unknown", "Xmodem", "Zmodem", "ZedZap", "DirZap",
	"Janus", "Hydra", "Tcp", "BinkP"
};

/*****************************************************************************
 * Get first file from files queue not marked as sent. Extract files in the
 * order: netmail, arcmail, other files, file request. Files of equal type
 * extract in the order of flavor descending
 *
 * Arguments:
 * 	dest      pointer to the pointer to put file information to
 * 	pi        pointer to the protocol information structure
 * 	q         pointer to the files queue
 *
 * Return value:
 * 	Zero value on success and non-zero if nothing to send
 */
static int prot_get_next_file(s_filelist **dest, s_protinfo *pi)
{
	s_filelist *ptrl = NULL;
	s_filelist *best = NULL;
	s_fsqueue *q = &state.queue;

	*dest = NULL;

	/* local queue */
	for( ptrl = q->fslist; ptrl; ptrl = ptrl->next )
		if( ptrl->status == STATUS_WILLSEND )
		{
			if( pi->reqs_only )
			{
				if( (ptrl->type & TYPE_REQUEST) )
				{
					*dest = ptrl;
					return 0;
				}
			}
			else if( best )
			{
				if( (ptrl->type & TYPE_NETMAIL) )
				{
					if( (best->type & TYPE_NETMAIL) )
					{
						if( ptrl->flavor > best->flavor )
							best = ptrl;
					} else
						best = ptrl;
				}
				else if( (ptrl->type & TYPE_ARCMAIL) )
				{
					if( (best->type & TYPE_ARCMAIL) && (ptrl->flavor > best->flavor) )
						best = ptrl;
					else if( !(best->type & TYPE_NETMAIL)
					      && !(best->type & TYPE_ARCMAIL) )
						best = ptrl;
				}
				else if( !(ptrl->type & TYPE_REQUEST) && (best->type & TYPE_REQUEST) )
					best = ptrl;
			}
			else
				best = ptrl;
		}
	
	if( best )
	{
		*dest = best;
		return 0;
	}
	
	for( ptrl = pi->filelist; ptrl; ptrl = ptrl->next )
		if( ptrl->status == STATUS_WILLSEND )
		{
			*dest = ptrl;
			return 0;
		}
	
	/* network queue */
#ifdef NETSPOOL

	/*log("netspool next file");*/
	if(state.netspool.state == NS_NOTINIT) {
	    /*log("new netspool connection");*/
	    char password[100];
	    char address[300];
	    char *host = conf_string(cf_netspool_host);
	    char *port = conf_string(cf_netspool_port);
	    if(host==NULL) {
		log("netspool is not configured");
	        state.netspool.state = NS_UNCONF;
	    } else {
		snprintf(address, 299, state.node.addr.point? "%d:%d/%d.%d": "%d:%d/%d",
                        state.node.addr.zone, state.node.addr.net,
                        state.node.addr.node, state.node.addr.point);
		if(state.protected) {
		    session_get_password(state.node.addr, password, 100);
		} else {
		    password[0] = 0;
		}
		log("netspool start %s %s %s (pwd)", host, port, address);
		netspool_start(&state.netspool, host, port, address, password);
	    }
	}

	if(state.netspool.state == NS_READY) {
	    /*log("netspool request");*/
	    netspool_query(&state.netspool, "ALL");
	}

	if(state.netspool.state == NS_RECEIVING) {
	    /*log("netspool begin receive");*/
	    netspool_receive(&state.netspool);
	} else {
	    log("netspool could not start receive");
	    return 1;
	}

	if(state.netspool.state == NS_RECVFILE) {
	    /*log("netspool start file");*/
	    *dest = NULL;
	    return 0;
	}

	if(state.netspool.state == NS_READY) {
	    log("netspool queue empty");
	    netspool_end(&state.netspool);
	}

	if(state.netspool.state==NS_ERROR) {
	    log("no next file: netspool error %s", state.netspool.error);
	}

#endif

	return 1;
}

/*****************************************************************************
 * Calculate total files number and size we are going to send. It will be
 * used by some protocols that supports reporting of total traffic to the
 * remote side (e.g. Zmodem protocols could)
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 * 	q         pointer to the files queue
 *
 * Return value:
 * 	None
 */
/*
void prot_update_traffic(s_protinfo *pi, const s_fsqueue *q)
{
	s_filelist *ptrl = NULL;

	pi->send_bytesleft = 0;
	pi->send_filesleft = 0;

	if( !state.sopts.holdall )
	{
		for( ptrl = q->fslist; ptrl; ptrl = ptrl->next )
			if( ptrl->status == STATUS_WILLSEND )
			{
				pi->send_filesleft += 1;
				pi->send_bytesleft += ptrl->size;
			}
	
		for( ptrl = pi->filelist; ptrl; ptrl = ptrl->next )
			if( ptrl->status == STATUS_WILLSEND )
			{
				pi->send_filesleft += 1;
				pi->send_bytesleft += ptrl->size;
			}
	}
}
*/

/*****************************************************************************
 * Open next file for sending. Set all necessary information. And do all
 * necessary checks.
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	Zero value on success and non-zero if have nothing to send
 */
int p_tx_fopen(s_protinfo *pi)
{
	FILE *fp;
	struct stat st;
	s_filelist *ptrl = NULL;

	if( state.sopts.holdall )
		return 1;

get_next_file:
	if( prot_get_next_file(&ptrl, pi) )
		return 1;

	if( ptrl ) {
	
	    /* Mark this file as "processed" */
	    ptrl->status = STATUS_SENDING;

	    pi->send_left_num  -= 1;
	    pi->send_left_size -= ptrl->size;
	
	    if( pi->send_left_size < 0 )
		pi->send_left_size = 0;
	    if( pi->send_left_num < 0 )
		pi->send_left_num = 0;

	    DEB((D_PROT, "p_tx_fopen: now opening \"%s\"", ptrl->fname));
	
	    if( stat(ptrl->fname, &st) ) {
		logerr("send: cannot stat file \"%s\"", ptrl->fname);
		goto get_next_file;
	    }
	
	    if( (fp = file_open(ptrl->fname, "r")) == NULL ) {
		logerr("send: cannot open file \"%s\"", ptrl->fname);
		goto get_next_file;
	    }

	}
#ifndef NETSPOOL
	else {
	    return 1;
	}
#endif

	/* Reset MinCPS time counter */
	pi->tx_low_cps_time = 0;

	/*
	 * Add new entry to the send files queue
	 */
	if( pi->sentfiles && pi->n_sentfiles > 0 )
	{
		pi->sentfiles = (s_finfo *)xrealloc(pi->sentfiles, sizeof(s_finfo)*(pi->n_sentfiles+1));
		memset(&pi->sentfiles[pi->n_sentfiles], '\0', sizeof(s_finfo));
		pi->send = &pi->sentfiles[pi->n_sentfiles++];
	}
	else
	{
		pi->sentfiles = (s_finfo *)xmalloc(sizeof(s_finfo));
		memset(pi->sentfiles, '\0', sizeof(s_finfo));
		pi->send = pi->sentfiles;
		pi->n_sentfiles = 1;
	}
	
	/*
	 * Set file information
	 */
#ifdef NETSPOOL
	if( !ptrl ) {
	    /* send file received through netspool */
	pi->send->fp          = 0;
	pi->send->local_name  = "NETSPOOL";
	pi->send->type        = out_filetype(state.netspool.filename);
	pi->send->net_name    = recode_file_out(p_convfilename(state.netspool.filename, pi->send->type));
	pi->send->fname       = NULL;
	pi->send->mod_time    = time(NULL);
	pi->send->mode        = 0664;
	pi->send->bytes_total = state.netspool.length;
	pi->send->start_time  = time(NULL);
	pi->send->eofseen     = FALSE;
	pi->send->status      = FSTAT_PROCESS;
	pi->send->action      = ACTION_ACKNOWLEDGE;
	pi->send->flodsc      = -1;
	} else {

#endif
	pi->send->fp          = fp;
	pi->send->local_name  = (char*)xstrcpy(file_getname(ptrl->fname));
	pi->send->net_name    = recode_file_out(p_convfilename(pi->send->local_name, ptrl->type));
	pi->send->fname       = (char*)xstrcpy(ptrl->fname);
	pi->send->mod_time    = (ptrl->type & TYPE_REQUEST) ? time(NULL) : st.st_mtime;
	pi->send->mod_time    = localtogmt(pi->send->mod_time);
	pi->send->mod_time   += pi->send->mod_time%2;
	pi->send->mode        = st.st_mode;
	pi->send->bytes_total = st.st_size;
	pi->send->start_time  = time(NULL);
	pi->send->eofseen     = FALSE;
	pi->send->status      = FSTAT_PROCESS;
	pi->send->type        = ptrl->type;
	pi->send->action      = ptrl->action;
	pi->send->flodsc      = ptrl->flodsc;

#ifdef NETSPOOL
	}
#endif

	if( strcmp(pi->send->local_name, pi->send->net_name) == 0 )
	{
		log("send: \"%s\" %d bytes",
			pi->send->local_name,
			pi->send->bytes_total);
	}
	else
	{
		log("send: \"%s\" %d bytes -> \"%s\"",
			pi->send->local_name,
			pi->send->bytes_total,
			pi->send->net_name);
	}

	return 0;
}

void prot_traffic_update(s_traffic *traf, size_t size, int xtime, int type)
{
	if( type & TYPE_REQANSW )
	{
		traf->freqed_size += size;
		traf->freqed_time += xtime;
		traf->freqed_num++;
	}
	else if( type & TYPE_NETMAIL )
	{
		traf->netmail_size += size;
		traf->netmail_time += xtime;
		traf->netmail_num++;
	}
	else if( type & TYPE_ARCMAIL )
	{
		traf->arcmail_size += size;
		traf->arcmail_time += xtime;
		traf->arcmail_num++;
	}
	else
	{
		traf->files_size += size;
		traf->files_time += xtime;
		traf->files_num++;
	}
}

size_t prot_traffic_total_size(const s_traffic *traff)
{
	return traff->netmail_size + traff->arcmail_size
	     + traff->files_size + traff->freqed_size;
}

int prot_traffic_total_num(const s_traffic *traff)
{
	return traff->netmail_num + traff->arcmail_num
	     + traff->files_num + traff->freqed_num;
}

/*****************************************************************************
 * Close sending file. If file was sent successfully than make appropriate
 * actions (e.g. unlink/truncate file)
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	Zero value on success and non-zero to indicate error
 */
int p_tx_fclose(s_protinfo *pi)
{
	long trans_time = 0;
	long cps = 0;
	
	if( pi->send->fp )
	{
		(void)file_close(pi->send->fp);
		pi->send->fp = NULL;
	}
	
	/*
	 * Calculate send time, CPS, etc
	 */
	trans_time = (long)time(NULL) - (long)pi->send->start_time;
	
	if( trans_time > 0 )
		cps = (long)((pi->send->bytes_sent - pi->send->bytes_skipped) / trans_time);
	
	log("sent: \"%s\" %ld [%s]: %ld seconds, %ld CPS",
		pi->send->net_name,
		(long)(pi->send->bytes_sent - pi->send->bytes_skipped),
		(pi->send->status == FSTAT_SKIPPED) ? "skipped" :
		(pi->send->status == FSTAT_REFUSED) ? "refused" :
		(pi->send->status == FSTAT_SUCCESS) ? "ok" : "error",
		(long)(trans_time > 0) ? trans_time : 0,
		(long)(cps > 0) ? cps : 0);
	
	/*
	 * Update outgoing traffic structure
	 */
	prot_traffic_update(&pi->traffic_sent,
		pi->send->bytes_sent - pi->send->bytes_skipped,
		trans_time, pi->send->type);
	
	/*
	 * Calculate time of sending FREQ'ed files
	 */
	if( (pi->send->type & TYPE_REQANSW) == TYPE_REQANSW )
		pi->send_freqtime += trans_time;
	
	/*
	 * Do necessary actions
	 */
	if( pi->send->status == FSTAT_SUCCESS || pi->send->status == FSTAT_SKIPPED )
	{
		FILE *tfp;

		if( (pi->send->type & TYPE_FROMFLO) )
			out_flo_marksent(pi->send->fname,
				state.queue.flotab[pi->send->flodsc].fname);
		
		switch(pi->send->action) {
		case ACTION_NOTHING:
			DEB((D_PROT, "p_tx_fclose: do nothing with file \"%s\"", pi->send->fname));
			break;
			
		case ACTION_UNLINK:
		case ACTION_FORCEUNLINK:
			DEB((D_PROT, "p_tx_fclose: unlinking file \"%s\"", pi->send->fname));
			if( unlink(pi->send->fname) && errno != ENOENT )
				logerr("send: cannot unlink file \"%s\"", pi->send->fname);
			break;
			
		case ACTION_TRUNCATE:
			DEB((D_PROT, "p_tx_fclose: truncating file \"%s\"", pi->send->fname));
			if( (tfp = fopen(pi->send->fname, "w")) )
				fclose(tfp);
			else if( errno != ENOENT )
				logerr("send: cannot truncate file \"%s\"", pi->send->fname);
			break;
#ifdef NETSPOOL
		case ACTION_ACKNOWLEDGE:
			log("netspool commit %s", state.netspool.filename);
			netspool_acknowledge(&state.netspool);
			break;
#endif
		}
	}
	
	return 0;
}

/*****************************************************************************
 * Read next part of file to the buffer. User by transmitter.
 *
 * Arguments:
 * 	buffer    pointer to the buffer
 * 	buflen    buffer size
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	>= 0 - number of bytes read
 * 	  -1 - error reading file (must try to send file later)
 * 	  -2 - file must be skipped (send never)
 */
int p_tx_readfile(char *buffer, size_t buflen, s_protinfo *pi)
{
	int n;
	struct stat st;
	long ftell_pos;

#ifdef NETSPOOL
	if(pi->send->fname==NULL && strcmp(pi->send->local_name, "NETSPOOL")==0 ) {
	    /*log("reading netspool file");*/
	    if( state.netspool.state != NS_RECVFILE ) {
		log("send: wrong netspool state");
		pi->send->status = FSTAT_SKIPPED;
		return -2;
	    }
	    n = netspool_read(&state.netspool, buffer, buflen);
	    pi->send->eofseen = state.netspool.length == 0;
	    if( n==-1 ) {
		log("send: netspool error %s", state.netspool.error);
		pi->send->status = FSTAT_SKIPPED;
		return -2;
	    }
	    /*log("got %d bytes from netspool", n);*/
	    return n;
	} else {
	    /*log("reading local file");*/
	}
#endif
	/*
	 * Sanity check: read from closed file.
	 */
	if (pi->send->fp == NULL) {
	  log ("Error: Read from closed file \"%s\".", pi->send->fname);
	  return -1;
	}
	ftell_pos = ftell(pi->send->fp);
	
	pi->send->eofseen = FALSE; /* clear EOF flag */
	
	/*
	 * Sanity check: sync our transmitter position
	 * with the position returned by the ftell()
	 */
	if( ftell_pos < 0 )
	{
		log("Error: ftell() return %ld for the file \"%s\"",
			ftell_pos, pi->send->fname);
		pi->send->status = FSTAT_REFUSED;
		return -1;
	}
	
	/*
	 * Try receive file later, if such error occurs
	 */
	if( pi->send->bytes_sent != ftell_pos )
	{
		log("internal error: invalid transmitting offset");
		log("pi->send->bytes_sent = %ld, ftell() = %ld",
			(long)pi->send->bytes_sent, ftell_pos);
		pi->send->bytes_received = ftell_pos;
		return -1;
	}
	
	if( stat(pi->send->fname, &st) == -1 && errno == ENOENT )
	{
		log("send: file not found! do you want to skip it?");
		pi->send->status = FSTAT_SKIPPED;
		return -2;
	}
	else if( (n = fread(buffer, 1, buflen, pi->send->fp)) < buflen )
	{
		if( feof(pi->send->fp) )
			pi->send->eofseen = TRUE;
		else
		{
			log("send: error reading file from disk (ferror = %d)",
				ferror(pi->send->fp));
			pi->send->status = FSTAT_REFUSED;
			return -1;
		}
	}
	
	return n;
}

/*****************************************************************************
 * Write buffer to the file. Used by receiver.
 *
 * Arguments:
 * 	buffer    pointer to the buffer
 * 	buflen    number of bytes in buffer
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	 0 - successfully written
 * 	-1 - error writing file (receive later)
 * 	-2 - file must be skipped (receive never)
 */
int p_rx_writefile(const char *buffer, size_t buflen, s_protinfo *pi)
{
	struct stat st;
	long ftell_pos = ftell(pi->recv->fp);
	
	/*
	 * Sanity check: sync our receiver position
	 * with the position returned by the ftell()
	 */
	if( ftell_pos < 0 )
	{
		log("Error: ftell() return %ld for the file \"%s\"",
			ftell_pos, pi->recv->fname);
		pi->recv->status = FSTAT_REFUSED;
		return -1;
	}
	
	/*
	 * Try receive file later, if such error occurs
	 */
	if( pi->recv->bytes_received != ftell_pos )
	{
		log("internal error: invalid receiving offset");
		log("pi->recv->bytes_received = %ld, ftell() = %ld",
			(long)pi->recv->bytes_received, ftell_pos);
		pi->recv->bytes_received = ftell_pos;
		return -1;
	}
	
	if( stat(pi->recv->fname, &st) == -1 )
	{
		fflush(pi->recv->fp);
		if( stat(pi->recv->fname, &st) == -1 && errno == ENOENT )
		{
			log("recv: skip file \"%s\" (file not found)", pi->recv->fname);
			pi->recv->status = FSTAT_SKIPPED;
			return -2;
		}
	}
	
	if( buflen > 0 && fwrite(buffer, buflen, 1, pi->recv->fp) != 1 )
	{
		log("recv: error writing file \"%s\" to disk (ferror = %d)",
			pi->recv->fname, ferror(pi->recv->fp));
		pi->recv->status = FSTAT_REFUSED;
		return -1;
	}
	
	return 0;
}

/*****************************************************************************
 * Move file from temporary inbound to main
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	0 - successfully processed file
 * 	1 - error occured (mailer must refuse this file (receive later))
 */
static int p_move2inbound(s_protinfo *pi)
{
	int  rc = 0;
	char *realname = NULL;
	char *uniqname = NULL;
	char *destname = NULL;

	if( state.inbound && pi->recv->local_name )
	{
		realname = (char *)xstrcpy(state.inbound);
		realname = (char *)xstrcat(realname, pi->recv->local_name);
	} else
		return 1;

	if( access(realname, F_OK) == -1 )
	{
		destname = realname;
	}
	else
	{
		if( (uniqname = prot_unique_name(state.inbound,
				pi->recv->local_name, pi->recv->type)) == NULL )
		{
			log("recv: cannot get unique name for \"%s\"",
				pi->recv->local_name);
			free(realname);
			return 1;
		}

		if( !strcmp(realname, uniqname) )
		{
			log("recv: overwriting \"%s\"",
				file_getname(uniqname));
		}
		else
		{
			log("recv: rename \"%s\" -> \"%s\"",
				pi->recv->local_name, file_getname(uniqname));
		}
		
		destname = uniqname;
	}
	
	if( (rc = rename(pi->recv->fname, destname)) == -1 )
	{
		logerr("recv: cannot rename \"%s\" -> \"%s\"", 
			pi->recv->fname, destname);
	}
	else
	{
		mode_t fmode = 0;
		
		/*
		 * Change new file permissions
		 */
		if( (pi->recv->type & TYPE_NETMAIL) == TYPE_NETMAIL )
			fmode = conf_filemode(cf_mode_netmail);
		else if( (pi->recv->type & TYPE_ARCMAIL) == TYPE_ARCMAIL )
			fmode = conf_filemode(cf_mode_arcmail);
		else if( (pi->recv->type & TYPE_REQUEST) == TYPE_REQUEST )
			fmode = conf_filemode(cf_mode_request);
		else if( (pi->recv->type & TYPE_TICFILE) == TYPE_TICFILE )
			fmode = conf_filemode(cf_mode_ticfile);

		if( fmode == 0 )
			fmode = conf_filemode(cf_mode_default);
		
		if( chmod(destname, fmode ? fmode : (S_IRUSR|S_IWUSR)) == -1 )
			logerr("recv: cannot set permissions for file \"%s\"",
				destname);
	}
	
	if( realname )
		free(realname);
	if( uniqname )
		free(uniqname);
	
	return rc ? 1 : 0;
}

/*****************************************************************************
 * Compare the two file informations. One specified by file information
 * structure (s_finfo) and another specified by other variables.
 *
 * Arguments:
 * 	finf      pointer to the file information
 * 	fn        file name
 * 	sz        file size
 * 	tm        file modification time
 *
 * Return value:
 * 	Zero value if two files with such parameters seems to be the same
 * 	file, and non-zero if it is different files
 */
int p_compfinfo(s_finfo *finf, const char *fn, size_t sz, time_t tm)
{
	return ( sz == finf->bytes_total && tm == finf->mod_time
	         && strcmp(fn, finf->net_name) == 0 ) ? 0 : 1;
}

/*****************************************************************************
 *  Open file for receiving. If file with such name allready exist:
 *  1) if timestamp and size identical to existing one, then skip;
 *  2) if only timestamp is equal and size of existing file is shorter then
 *     it possible was an unfinished transfer - try to recover;
 *  3) in all other cases rename receiving file.
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 * 	fn        file name                   |
 * 	sz        file size                   | \ as it was reported by
 * 	tm        file last modification time | / remote side (need checks)
 * 	mode      file permissions            |
 *
 * Return value:
 * 	0 - successful call
 * 	1 - refuse this file (receive later)
 * 	2 - skip this file (never receive it)
 */
int p_rx_fopen(s_protinfo *pi, char *fn, size_t sz, time_t tm, mode_t mode)
{
	const char *openmode = "w";	/* Set open mode to overwrite */
	struct stat st;
	const char *p_skiplist  = NULL;
	const char *p_delaylist = NULL;
	size_t minfree = 0;
	size_t needed_bytes_total = 0;
	
	/*
	 * Check. May be we are receiving this file allready
	 */
	if( pi->recv && p_compfinfo(pi->recv, fn, sz, tm) == 0 )
	{
		log("recv: got duplicated file information");
		
		if( pi->recv->fp )
			return 0;
		else if( pi->recv->status == FSTAT_SKIPPED )
			return 2;
		else if( pi->recv->status == FSTAT_REFUSED )
			return 1;
	}
	
	/* Reset mincps time counter */
	pi->rx_low_cps_time = 0;
	
	/*
	 * Add new entry to the receive files queue
	 */
	if( pi->rcvdfiles && pi->n_rcvdfiles > 0 )
	{
		pi->rcvdfiles = (s_finfo *)xrealloc(pi->rcvdfiles, sizeof(s_finfo)*(pi->n_rcvdfiles+1));
		memset(&pi->rcvdfiles[pi->n_rcvdfiles], '\0', sizeof(s_finfo));
		pi->recv = &pi->rcvdfiles[pi->n_rcvdfiles++];
	}
	else
	{
		pi->rcvdfiles = (s_finfo *)xmalloc(sizeof(s_finfo));
		memset(pi->rcvdfiles, '\0', sizeof(s_finfo));
		pi->recv = pi->rcvdfiles;
		pi->n_rcvdfiles = 1;
	}

	/*
	 * Set file information
	 */
	pi->recv->net_name    = (char*)xstrcpy(fn);
	pi->recv->local_name  = xstrcpy(file_getname(fn));
	pi->recv->bytes_total = sz;
	pi->recv->mod_time    = tm;
	pi->recv->mode        = mode;
	pi->recv->start_time  = time(NULL);
	pi->recv->eofseen     = FALSE;
	pi->recv->status      = FSTAT_PROCESS;
	pi->recv->type        = out_filetype(pi->recv->net_name);
	pi->recv->action      = ACTION_NOTHING;
	pi->recv->flodsc      = 0;

	/*
	 * Convert file name to local charset
	 */
	recode_file_in(pi->recv->local_name);

	/*
	 * Remove undesirable characters from file name
	 */
	file_name_makesafe(pi->recv->local_name);

	/*
	 * Upper case file names convert to lower case.
	 */
	if( (string_isupper(pi->recv->local_name)) && conf_boolean(cf_recieved_to_lower))
		string_tolower(pi->recv->local_name);
	
	if( strcmp(pi->recv->local_name, pi->recv->net_name) )
	{
		log("recv: \"%s\" %d bytes -> \"%s\"",
			string_printable(pi->recv->net_name),
			pi->recv->bytes_total,
			string_printable(pi->recv->local_name));
	}
	else
	{
		log("recv: \"%s\" %d bytes",
			string_printable(pi->recv->local_name),
			pi->recv->bytes_total);
	}
	
	/*
	 *  Get `DelayFilesIn', `Skipfiles' and `MinFree' values
	 */
	p_skiplist  = conf_string(cf_skip_files_recv);
	p_delaylist = conf_string(cf_delay_files_recv);
	minfree     = conf_number(cf_min_free_space);
	
	/*
	 *  Should we delay(refuse) this file NOW?
	 */
	if( p_delaylist && !checkmasks(p_delaylist, pi->recv->net_name) )
	{
		log("recv: file from delaylist -> refuse");
		pi->recv->status = FSTAT_REFUSED;
		return(1);
	}

	/*
	 *  Should we skip this file NOW?
	 */
	if( p_skiplist && !checkmasks(p_skiplist, pi->recv->net_name) )
	{
		log("recv: file from skiplist -> skip");
		pi->recv->status = FSTAT_SKIPPED;
		return(2);
	}
	
	/*
	 * Check. May be we allready have this file
	 */
	if( (pi->recv->type & TYPE_REQUEST) || (pi->recv->type & TYPE_NETMAIL) )
	{
		pi->recv->fname = p_gettmpname(state.tinbound, pi->recv->local_name,
	                                   state.node.addr, pi->recv->bytes_total,
	                                   pi->recv->mod_time);
	}
	else /* It is not netmail or filerequest */
	{
		char *fname;
		fname = (char *)xstrcpy(state.inbound);
		fname = (char *)xstrcat(fname, pi->recv->local_name);

		if( stat(fname, &st ) == 0 )
		{
			/* Skip file if we allready have it's copy */
			if( pi->recv->mod_time == localtogmt(st.st_mtime)
			 && pi->recv->bytes_total == st.st_size )
			{
				log("recv: allready have \"%s\"", fname);
				pi->recv->status = FSTAT_SKIPPED;
			}
		}
	
		free(fname); fname = NULL;
		
		if( pi->recv->status == FSTAT_SKIPPED )
			return 2;
		
		pi->recv->fname = p_gettmpname(state.tinbound, pi->recv->local_name,
	                                   state.node.addr, pi->recv->bytes_total,
	                                   pi->recv->mod_time);
		
		/* Now make same check for temporary inbound */
		if( stat(pi->recv->fname, &st) == 0 )
		{
			/* Skip if it is the same file */
			if( pi->recv->mod_time == localtogmt(st.st_mtime)
			 && pi->recv->bytes_total == st.st_size )
			{
				/*
				 * We will skip it and try to move it into inbound
				 * directory, do you know who could left it here ? :)
				 */
				
				log("recv: allready have \"%s\"", pi->recv->fname);
				
				if( p_move2inbound(pi) == 0 )
					pi->recv->status = FSTAT_SKIPPED;
				else	
					pi->recv->status = FSTAT_REFUSED;
				
				free(pi->recv->fname);
				pi->recv->fname = NULL;
				
				return pi->recv->status == FSTAT_SKIPPED ? 2 : 1;
			}
			else if( pi->recv->mod_time == localtogmt(st.st_mtime)
			      && pi->recv->bytes_total >= st.st_size )
			{
				pi->recv->bytes_skipped  = st.st_size;
				pi->recv->bytes_received = pi->recv->bytes_skipped;
				
				log("recv: receiving \"%s\" from offset %ld",
					pi->recv->fname, (long)pi->recv->bytes_skipped);
				
				openmode = "a";
			}
		}
	}

	/*
	 *  Check, there is enough space in our inbound
	 */
	if (openmode == "a") needed_bytes_total = minfree + pi->recv->bytes_total - pi->recv->bytes_skipped;
		else 	     needed_bytes_total = minfree + pi->recv->bytes_total;

	if( minfree > 0 && getfreespace(state.inbound) < needed_bytes_total )
	{
		log("recv: not enough free space in inbound -> refuse");
		pi->recv->status = FSTAT_REFUSED;
		return(1);
	}
	
	DEB((D_PROT, "p_rx_fopen: call fopen(\"%s\", \"%s\")",
		pi->recv->fname, openmode));
		
	if( (pi->recv->fp = file_open(pi->recv->fname, openmode)) == NULL )
	{
		logerr("recv: cannot open \"%s\" -> refuse", pi->recv->fname);
		
		pi->recv->status = FSTAT_REFUSED;
		free(pi->recv->fname);
		pi->recv->fname = NULL;
		
		return 1;
	}

	if( pi->buffer && pi->buflen > 0 )
	{
#ifdef SETVBUF_REVERSED
		setvbuf(pi->recv->fp, _IOFBF, pi->buffer, pi->buflen);
#else
		setvbuf(pi->recv->fp, pi->buffer, _IOFBF, pi->buflen);
#endif
	}
	
	return 0;
}

/*****************************************************************************
 * Close receiving file. If it was completly received - move it to the main
 * inbound directory.
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	0 - successfull call
 * 	1 - error occured, refuse this file (receive later)
 */
int p_rx_fclose(s_protinfo *pi)
{
	long trans_time = 0;
	long cps = 0;
	int rc = 0;
	struct stat st;
	
	DEB((D_PROT, "p_rx_fclose: close file \"%s\"", pi->recv->fname));
	
	if( pi->recv->fp )
	{
		rc = file_close(pi->recv->fp); pi->recv->fp = NULL;
		
		if( rc )
		{
			/*
			 * This may be any sort of errors, including
			 * random data corruptions. Return error.
			 */
			logerr("recv: cannot close file \"%s\"", pi->recv->fname);
			return 1;
		}
	}
	
	if( pi->recv->status == FSTAT_SKIPPED )
	{
		/*
		 * Remove skipped file from temp. inbound
		 */
		if( unlink(pi->recv->fname) == -1 && errno != ENOENT )
			logerr("recv: cannot remove skipped file \"%s\"", pi->recv->fname);
	}
	else
	{
		/*
		 * Set original file modification time
		 */
		if( pi->recv->mod_time )
		{
			struct utimbuf tvp;
			
			tvp.actime  = time(NULL);
			tvp.modtime = gmttolocal(pi->recv->mod_time);
			utime(pi->recv->fname, &tvp);
		}
	
		/*
		 *  Set default file permission to 0600
		 */
		if( chmod(pi->recv->fname, (S_IRUSR | S_IWUSR)) == -1 )
			logerr("recv: cannot change mode for file \"%s\"", pi->recv->fname);
	}
	
	/*
	 * Update incoming traffic structure
	 */
	prot_traffic_update(&pi->traffic_rcvd,
		pi->recv->bytes_received - pi->recv->bytes_skipped,
		trans_time, pi->recv->type);
					
	/*
	 * If file was received successfuly
	 */
	if( pi->recv->status == FSTAT_SUCCESS )
	{
		/*
		 * Sanity check. Compare real file length with the
		 * expected length (reported by the remote side)
		 */
		if( stat(pi->recv->fname, &st) == -1 )
		{
			log("cannot stat received file \"%s\"", pi->recv->fname);
			pi->recv->status = FSTAT_REFUSED;
		}
		else if( st.st_size != pi->recv->bytes_total )
		{
			log("received file has invalid size %ld (%ld expected)",
				(long)st.st_size, (long)pi->recv->bytes_total);
			pi->recv->status = FSTAT_REFUSED;
		}
		else if( state.reqstat == REQS_ALLOW
		      && (pi->recv->type & TYPE_REQUEST) )
		{
			/* Run our freq processor */
			req_proc(pi->recv->fname, &pi->filelist);
			unlink(pi->recv->fname);
		}
		else if( p_move2inbound(pi) )
		{
			log("recv: can't move file into main inbound -> refuse");
			pi->recv->status = FSTAT_REFUSED;
		}
	}
	
	/*
	 * Calculate receive time, CPS, etc
	 */
	trans_time = (long)time(NULL) - (long)pi->recv->start_time;
	
	if( trans_time > 0 )
		cps = (long)((pi->recv->bytes_received - pi->recv->bytes_skipped) / trans_time);
	
	log("rcvd: \"%s\" %ld [%s]: %ld seconds, %ld CPS",
		pi->recv->local_name,
		(long)(pi->recv->bytes_received - pi->recv->bytes_skipped),
		(pi->recv->status == FSTAT_SKIPPED) ? "skipped" :
		(pi->recv->status == FSTAT_REFUSED) ? "refused" :
		(pi->recv->status == FSTAT_SUCCESS) ? "ok" : "error",
		(long)(trans_time > 0) ? trans_time : 0,
		(long)(cps > 0) ? cps : 0);
	
	return pi->recv->status == FSTAT_SUCCESS ? 0 : 1;
}

/*****************************************************************************
 * Check current receiving/sending speeds (CPS), check time limits.
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *	bidir	  if hydra, check only one CPS value
 *
 * Return value:
 * 	PRC_NOERROR   - all ok
 * 	PRC_STOPTIME  - abort session due to time limits
 * 	PRC_CPSTOOLOW - abort session due low speed
 */
int p_info(s_protinfo *pi, int bidir)
{
	int rc = 0, rx_cps_low = 0, tx_cps_low = 0;
	time_t tx_now, rx_now, now;

	now = time(NULL);
	
	if( pi->send && pi->send->status == FSTAT_PROCESS )
	{
		tx_now = (long)time(NULL) - (long)pi->send->start_time;
		pi->tx_cps = (pi->send->bytes_sent - pi->send->bytes_skipped) / (tx_now ? tx_now : 1);
		
		/* Is it FREQ answer? */
		if( pi->freq_timelimit > 0 && (pi->send->type & TYPE_REQANSW) )
		{
			/*
			 *  Check FREQ time limit not reached?
			 */
			if( pi->send_freqtime + tx_now > pi->freq_timelimit )
			{
				log("reached FREQ time limit %d second(s)", pi->freq_timelimit);
				rc = PRC_STOPTIME;
			}
		}
		
		/* Check is transmiting CPS to low */
		if( pi->min_tx_cps )
		{
			if( pi->tx_low_cps_time )
			{
				if( pi->tx_cps < pi->min_tx_cps )
				{
					if( now - pi->tx_low_cps_time >= pi->min_cps_time )
					{
						tx_cps_low = 1;
					}
				}
				else
				{
					pi->tx_low_cps_time = 0;
				}
			}
			else if( pi->tx_cps < pi->min_tx_cps )
			{
				pi->tx_low_cps_time = now;
			}
		}
	} /* end of if( txi ) */

	if( pi->recv && pi->recv->status == FSTAT_PROCESS )
	{
		rx_now = (long)time(NULL) - (long)pi->recv->start_time;
		pi->rx_cps = (pi->recv->bytes_received - pi->recv->bytes_skipped) / (rx_now ? rx_now : 1);
		
		/* Check is receiving CPS to low */
		if( pi->min_rx_cps )
		{
			if( pi->rx_low_cps_time )
			{
				if( pi->rx_cps < pi->min_rx_cps )
				{
					if( now - pi->rx_low_cps_time >= pi->min_cps_time )
					{
						rx_cps_low = 1;
					}
				}
				else
				{
					pi->rx_low_cps_time = 0;
				}
			}
			else if( pi->rx_cps < pi->min_rx_cps )
			{
				pi->rx_low_cps_time = now;
			}
		}
	} /* end of if( rxi ) */

	/* check maybe now is a good time to stop it ? =) */
	if( pi->stop_time && now >= pi->stop_time )
	{
		log("reached stop time");
		rc = PRC_STOPTIME;
	}

	if ( tx_cps_low )
	{
		if ( !bidir || rx_cps_low ) {
			log("transmitting speed below %d cps", pi->min_tx_cps);
		        rc = PRC_CPSTOOLOW;
		}
	}

	if ( rx_cps_low )
	{
		if ( !bidir || tx_cps_low ) {
			log("receiving speed below %d cps", pi->min_rx_cps);
		        rc = PRC_CPSTOOLOW;
		}
	}

	return(rc);
}

/*****************************************************************************
 * Log short session statistic. Output overall Send/Received bytes, avreage
 * CPS, on-line time
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	None
 */
void p_log_txrxstat(const s_protinfo *pi)
{
	char    abuf[BF_MAXADDRSTR+1];
	s_faddr tmpaddr      = state.node.addr;
	int     total_time   = time_elapsed(pi->start_time);
	int     files_sent   = prot_traffic_total_num(&pi->traffic_sent);
	int     files_rcvd   = prot_traffic_total_num(&pi->traffic_rcvd);
	size_t  bytes_sent   = prot_traffic_total_size(&pi->traffic_sent);
	size_t  bytes_rcvd   = prot_traffic_total_size(&pi->traffic_rcvd);
	size_t  total_traff  = bytes_sent + bytes_rcvd;
	size_t  total_cps    = (total_traff / (total_time ? total_time : 1));
	
	tmpaddr.domain[0] = '\0'; /* remove domain from address */
	
	log("%s, S/R %ld/%ld, %ld/%ld bytes in %ld seconds, %ld CPS",
		ftn_addrstr(abuf, tmpaddr),
		files_sent, files_rcvd,
		(long)bytes_sent, (long)bytes_rcvd,
		(long)((total_time > 0) ? total_time : 0),
		(long)((total_cps  > 0) ? total_cps  : 0));
}

/*****************************************************************************
 * Do session cleanup. Remove temporary files, remove useless file requests
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	None
 */
void p_session_cleanup(s_protinfo *pi, bool success)
{
	int i;
	s_filelist *ptrl;
	
	if( success )
	{
		/*
		 * Remove sent file requests after successfull sessions
		 */
		for( i = 0; i < pi->n_sentfiles; i++ )
			if( (pi->sentfiles[i].type & TYPE_REQUEST)
			 && (pi->sentfiles[i].status == FSTAT_SUCCESS) )
			{
				if( unlink(pi->sentfiles[i].fname) == -1 && errno != ENOENT )
				{
					logerr("can't unlink '.req' file \"%s\"",
							pi->sentfiles[i].fname);
				}
			}
	}
	
	/*
	 * Delete files with action ACTION_FORCEUNLINK
	 */
	for( ptrl = pi->filelist; ptrl; ptrl = ptrl->next )
	{
		/* It must be allready deleted if it is sent */
		if( ptrl->action == ACTION_FORCEUNLINK && ptrl->status != STATUS_SENT )
		{
			if( unlink(ptrl->fname) && errno != ENOENT )
				logerr("cannot unlink temporary file \"%s\"", ptrl->fname);
		}
	}
	
#ifdef NETSPOOL
	if(state.netspool.state==NS_ERROR) {
	    log("end session: netspool error %s", state.netspool.error);
	}
	/*netspool_end(&state.netspool);*/
#endif
}

/*****************************************************************************
 * Do all necessary convertions and checks with file names before sending it
 *
 * Arguments:
 * 	origname  pointer to the original file name
 * 	type      file type (netmail, arcmail, etc.)
 *
 * Return value:
 * 	Pointer to the new file name (must be freed)
 */
char *p_convfilename(const char *origname, int type)
{
	const char *p;
	char *dest = NULL;
	long crc = 0L;
	s_faddr addr;

	if( (type & TYPE_NETMAIL) )
	{
		/*
		 *  Generate random file name for netmail packets
		 */
		dest = (char*)xmalloc(32);
		sprintf(dest, "%08lx.pkt", (long)rand());
	}
	else if( (type & TYPE_ASONAME) == TYPE_ASONAME
	      && (type & TYPE_ARCMAIL) == TYPE_ARCMAIL )
	{
		/*
		 *  Generate new file name for arcmail from AmigaDos style
		 *  outbound, such files looks like "2.5020.1682.9.fr1".
		 */
		if( (p = strrchr(origname, '.')) )
			crc = getcrc32ccitt(origname, p - origname - 1);
		else
			crc = getcrc32ccitt(origname, strlen(origname));
		
		dest = (char*)xmalloc(32);
		sprintf(dest, "%08lx%s", (long)crc, p?p:".fr0");
	}
	else if( (type & TYPE_ASONAME) == TYPE_ASONAME
	      && (type & TYPE_REQUEST) == TYPE_REQUEST )
	{
		/*
		 *  Generate new file name for file request from AmigaDos
		 *  style outbound, such files looks like "2.5020.1682.0.req".
		 */
		dest = (char*)xmalloc(32);
		if( out_parse_name_aso(&addr, origname) == 0 )
			sprintf(dest, "%04x%04x.req", addr.net, addr.node);
		else
			sprintf(dest, "%08lx.req", (long)rand());
	}
	else
	{
		/*
		 *  All other files send with their real name, but:
		 *  1) remove "nonprintable" characters
		 *  2) convert files to 8+3 format if remote has no long
		 *     file names support (indicated by $state.sopts.fnc)
		 */
		if( state.sopts.fnc )
		{
			dest = (char*)xmalloc(13);
			file_get_dos_name(dest, origname);
		}
		else
		{
			dest = (char*)xstrcpy(origname);
			if( string_isupper(dest) )
				string_tolower(dest);
		}
	}
	
	return dest;
}

/*****************************************************************************
 * Get temporary file name with full path.
 *
 * Arguments:
 * 	inbound   path to temporary inbound directory
 * 	filename  file name
 * 	addr      remote address \
 * 	sz        file size       > Used to generate unique file names
 * 	tm        file time      /
 *
 * Return value:
 * 	Pointer to the temporary file name (must be freed)
 */
char *p_gettmpname(const char *inbound, char *filename, s_faddr addr,
                   size_t sz, time_t tm)
{
	char *dest;
	char abuf[BF_MAXADDRSTR+1];
	char bbuf[BF_MAXADDRSTR+30];
	char cbuf[16];
	
	sprintf(bbuf, "%s %lx %lx", ftn_addrstr(abuf, addr), (long)sz, (long)tm);
	sprintf(cbuf, "%lx", (long)getcrc32ccitt(bbuf, strlen(bbuf)));
	
	dest = xstrcpy(inbound);
	dest = xstrcat(dest, filename);
	dest = xstrcat(dest, ".");
	dest = xstrcat(dest, cbuf);
	
	return dest;
}

#define MAX_TRIES 1024

/*****************************************************************************
 * Get new file name for received file. Use it when we allready have
 * file with such name in inbound directory.
 *
 * Rename file with the next algorithm:
 * 
 * -  For file request. Overwrite existing file
 * 
 * -  For netmail or ticfile. Generate new unique
 *    file name. (1234abcde.pkt -> ab04d51f.pkt)
 * 
 * -  For arcmail. Rotate extension chars to make
 *    unique name. (057d63a6.su0 -> 057d63a6.su1)
 * 
 * -  For other files. Try to add a numeric value
 *    as additional extension ("net5020.ndl.1")
 *    
 * Arguments:
 * 	dirname   pointer to the directory (inbound)
 * 	fname     pointer to the file name
 * 	type      file type (netmail, arcmail, etc.)
 *
 * Return value:
 * 	Pointer to the new file name with path (must be freed), and
 * 	NULL to indicate error
 */
char *prot_unique_name(char *dirname, char *fname, int type)
{
	int try  = 0;
	int offs = 0;
	char *result = NULL;
	char *p;

	if( (type & TYPE_REQUEST) )
	{
		result = (char *)xstrcpy(dirname);
		result = (char *)xstrcat(result, fname);
	}
	else if( (type & TYPE_NETMAIL) || (type & TYPE_TICFILE) )
	{
		char *ext = (type & TYPE_NETMAIL) ? "pkt" : "tic";
		
		offs = strlen(dirname);
		result = (char *)xmalloc(offs + 36);
		strncpy(result, dirname, offs + 35);
		result[offs + 35] = '\0';
				
		do
		{
			++try;
			sprintf(result+offs, "%08lx.%s", (long)rand(), ext);
		}
		while( !access(result, F_OK) && try < MAX_TRIES );
	}
	else if( (type & TYPE_ARCMAIL) )
	{
		result = (char *)xstrcpy(dirname);
		result = (char *)xstrcat(result, fname);

		p = result + strlen(result) - 1;

		do
		{
			++try;

			if( (*p >= '0') && (*p <= '8') )
				++*p;
			else if( (*p >= 'A') && (*p <= 'Y') )
				++*p;
			else if( (*p >= 'a') && (*p <= 'y') )
				++*p;
			else if( (*p == '9') )
				*p = 'a';
			else if( (*p == 'z') )
				*p = 'A';
			else if( --p < result || *p == '.' || *p == '/' )
			{
				free(result);
				result = NULL;
				break;
			}
		}
		while( !access(result, F_OK) && try < MAX_TRIES );
	}
	else
	{
		result = (char *)xstrcpy(dirname);
		result = (char *)xstrcat(result, fname);
		
		offs = strlen(result);
		
		result = (char *)xrealloc(result, offs + 16);
	
		if( result[offs-1] != '.' )
		{
			result[offs++] = '.';
			result[offs  ] = '\0';
		}
		
		do
		{
			++try;
			sprintf(result+offs, "%d", try);
		}
		while( !access(result, F_OK) && try < MAX_TRIES );
	}
	
	if( try >= MAX_TRIES )
	{
		if( result )
			free(result);
		
		return NULL;
	}
	
	return result;
}

/*****************************************************************************
 * Release memory used by file information structure
 *
 * Arguments:
 * 	fi        pointer to the file information structure
 *
 * Return value:
 * 	None
 */
void deinit_finfo(s_finfo *fi)
{
	if( fi->fp )
		fclose(fi->fp);
	
	if( fi->local_name )
		free(fi->local_name);
	if( fi->net_name )
		free(fi->net_name);
	if( fi->fname )
		free(fi->fname);
	
	memset(fi, '\0', sizeof(s_finfo));
}

/*****************************************************************************
 * Init protocol information structure. Set all necessary options, such as
 * minimal CPSes, start/stop time, etc.
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 * 	caller    are we caller?
 *
 * Return value:
 * 	None
 */
void init_protinfo(s_protinfo *pi, bool caller)
{
	long tmp;
	long sesslimit;

	
	memset(pi, '\0', sizeof(s_protinfo));
	
	pi->start_time = time(NULL);


	if( (pi->buflen = conf_number(cf_recv_buffer_size)) > 0 )
		pi->buflen = (pi->buflen / 2048) * 2048;
	else
		pi->buflen = 32768;
	
	if( pi->buflen > 0 )
		pi->buffer = (char*)xmalloc(pi->buflen);
	
	if( (tmp = conf_number(cf_min_cps_time)) > 0 )
		pi->min_cps_time = tmp;
	else
		pi->min_cps_time = 60;
	

	/*
	 * Set abort time if session limit was specified
	 */
	if( caller )
		sesslimit = conf_number(cf_session_limit_out);
	else
		sesslimit = conf_number(cf_session_limit_in);
	
	if( sesslimit > 0 )
		pi->stop_time = time(0) + sesslimit;
	
	/*
	 * Set minimal transmit speed (CPS)
	 */
	if( (tmp = conf_number(cf_min_cps_send)) > 0 )
		pi->min_tx_cps = tmp;
	else
	{
		if( state.handshake->protocol == PROT_ZMODEM
		 || state.handshake->protocol == PROT_ZEDZAP
		 || state.handshake->protocol == PROT_DIRZAP )
			tmp = conf_connlist(cf_zmodem_mincps_send, state.connspeed);
		else if( state.handshake->protocol == PROT_HYDRA )
			tmp = conf_connlist(cf_hydra_mincps_send, state.connspeed);
		else
			tmp = 0;

		if( tmp > 0 )
			pi->min_tx_cps = tmp;
	}
	
	/*
	 * Set minimal receive speed (CPS)
	 */
	if( (tmp = conf_number(cf_min_cps_recv)) > 0 )
		pi->min_rx_cps = tmp;
	else
	{
		if( state.handshake->protocol == PROT_ZMODEM
		 || state.handshake->protocol == PROT_ZEDZAP
		 || state.handshake->protocol == PROT_DIRZAP )
			tmp = conf_connlist(cf_zmodem_mincps_recv, state.connspeed);
		else if( state.handshake->protocol == PROT_HYDRA )
			tmp = conf_connlist(cf_hydra_mincps_recv, state.connspeed);
		else
			tmp = 0;

		if( tmp > 0 )
			pi->min_rx_cps = tmp;
	}

	/*
	 * Report about selected limits
	 */
	if( sesslimit > 0 && (pi->min_tx_cps > 0 || pi->min_rx_cps > 0) )
		log("use restrictions: %d/%d cps, %d seconds", pi->min_tx_cps, pi->min_rx_cps, sesslimit);
	else if( pi->min_tx_cps > 0 || pi->min_rx_cps > 0 )
		log("use restrictions: %d/%d cps", pi->min_tx_cps, pi->min_rx_cps);
	else if( sesslimit > 0 )
		log("use restrictions: %d seconds", sesslimit);
	
	DEB((D_PROT, "protinfo_init: min_tx_cps = %ld, min_rx_cps = %ld",
		(long)pi->min_tx_cps, (long)pi->min_rx_cps));
	DEB((D_PROT, "protinfo_init: buffersize = %ld, stoptime = %ld",
		(long)pi->min_tx_cps, (long)pi->min_rx_cps));
}

/*****************************************************************************
 * Release memory used by protocol information structure
 *
 * Arguments:
 * 	pi        pointer to the protocol information structure
 *
 * Return value:
 * 	None
 */
void deinit_protinfo(s_protinfo *pi)
{
	int i;
	
	for( i = 0; i < pi->n_sentfiles; i++ )
		deinit_finfo(&pi->sentfiles[i]);

	for( i = 0; i < pi->n_rcvdfiles; i++ )
		deinit_finfo(&pi->rcvdfiles[i]);
	
	if( pi->buffer )
		free(pi->buffer);
	if( pi->sentfiles )
		free(pi->sentfiles);
	if( pi->rcvdfiles )
		free(pi->rcvdfiles);
	
	memset(pi, '\0', sizeof(s_protinfo));
}
