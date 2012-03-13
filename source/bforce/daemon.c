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
#include "nodelist.h"
#include "io.h"
#include "session.h"
#include "outbound.h"
#include "daemon.h"

/*
 * Maxumum number of simultaneously running clients
 */
int max_tcpip = 0;
int max_modem = 0;

/*
 * Current number of running clients
 */
int tcpip_clients = 0;
int modem_clients = 0;

/*
 * Write queue/demon status to the log every XX seconds
 */
#define DAEMON_ALIVE_TIMER 1200

/*
 * Time to sleep than all queues are in "DQ_Idle" state
 */
#define DAEMON_IDLE_SLEEP 5

/*
 * Table of handled signals
 */
struct {
	int signum;
	DM_State state;
	bool wait;
} daemon_signals[] = {
	{ SIGHUP,      DM_Restart,       FALSE },
	{ SIGINT,      DM_Shutdown,      FALSE },
	{ SIGTERM,     DM_Shutdown,      FALSE },
	{ SIGUSR1,     DM_Usr1,          FALSE },
	{ SIGUSR2,     DM_Usr2,          FALSE },
	{ 0,           0,                FALSE }
};

/*
 * Systems queue (with all adresses, flavors, etc.)
 */
s_sysqueue daemon_sys_queue;

/*
 * Outgoing calls queues
 */
s_daemon_queue daemon_queues[] = {
	{ &daemon_sys_queue, -1, 0, FALSE, DQ_Idle }, /* Modem */
	{ &daemon_sys_queue, -1, 0, TRUE,  DQ_Idle }, /* IP */
	{ NULL,              -1, 0, FALSE, DQ_Idle }
};

/*
 * Positions of the certain queues in the 'daemon_queues' array
 */
#define MODEM_QUEUE 0
#define TCPIP_QUEUE 1

static RETSIGTYPE daemon_sighandler_chld(int sig)
{
	int old_errno = errno;
	int rc, status;
	pid_t pid;

	while( (pid = waitpid(-1, &status, WNOHANG)) > 0 )
	{
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		if( daemon_branch_exit(pid, rc) == -1 )
			log("wait got unexpected pid %d", (int)pid);
	}
	
	signal(SIGCHLD, daemon_sighandler_chld);
	errno = old_errno;
}

static RETSIGTYPE daemon_sighandler(int signum)
{
	int i;

	for( i = 0; daemon_signals[i].signum; i++ )
		if( daemon_signals[i].signum == signum )
		{
			daemon_signals[i].wait = TRUE;
			return;
		}

	log("daemon got unexpected signal %d", signum);
}

static int daemon_sysentry_init(s_sysentry *sysent)
{
	s_override *p;
	char msg[64] = "";
	char abuf[BF_MAXADDRSTR+1];
	
	/* Read calls/sessions statistic from sts file */
	(void)session_stat_get(&sysent->stat, &sysent->node.addr);
	
	/* Lookup node in nodelist */
	(void)nodelist_lookup(&sysent->node, sysent->node.addr);

	/* Set overrides */
	if( (p = conf_override(cf_override, sysent->node.addr)) )
	{
		sysent->overrides = p;
		sysent->lineptr = NULL;
		sysent->line = 0;
	}

	if( sysent->node.keyword == KEYWORD_HOLD )
	{
		strcpy(msg, "hold by nodelist");
		sysent->stat.undialable = TRUE;
	}
	else if( sysent->node.keyword == KEYWORD_DOWN )
	{
		strcpy(msg, "down by nodelist");
		sysent->stat.undialable = TRUE;
	}
	else if( sysent->stat.undialable )
		strcpy(msg, "undialable");
	else if( sysent->stat.hold_until )
	{
		int holdtime = sysent->stat.hold_until - time(0);

		if( holdtime > 0 )
			sprintf(msg, "calls holded for %d seconds", holdtime);
		else
			strcpy(msg, "calls holded");
	}
	else if( sysent->stat.hold_freqs )
	{
		int holdtime = sysent->stat.hold_freqs - time(0);

		if( holdtime > 0 )
			sprintf(msg, "freqs holded for %d seconds", holdtime);
		else
			strcpy(msg, "freqs holded");
	}

	if( msg && *msg )
		log("QUEUE: add %s (%s)", ftn_addrstr(abuf, sysent->node.addr), msg);
	else
		log("QUEUE: add %s", ftn_addrstr(abuf, sysent->node.addr));

	return 0;
}

static int daemon_sysentry_deinit(s_sysentry *sysent)
{
	char abuf[BF_MAXADDRSTR+1];
	
	log("QUEUE: remove %s", ftn_addrstr(abuf, sysent->node.addr));

	return 0;
}

static bool daemon_node_cancall_line(const s_node *node, const s_override *ovrd)
{
	bool good_phone = FALSE;
	bool good_host = FALSE;
	bool good_time = FALSE;
	time_t unixtime = time(NULL);
	struct tm *now = localtime(&unixtime);
	
	/*
	 * Check phone number
	 */
	if( ovrd && ovrd->sPhone && *ovrd->sPhone )
	{
		 if( modem_isgood_phone(ovrd->sPhone) )
			good_phone = TRUE;
	}
	else if( node && node->phone && *node->phone )
	{
		 if( modem_isgood_phone(node->phone) )
			good_phone = TRUE;
	}
	
	/*
	 * Check host name (IP address)
	 */
	if( ovrd && ovrd->sIpaddr && *ovrd->sIpaddr )
	{
		if( tcpip_isgood_host(ovrd->sIpaddr) )
			good_host = TRUE;
	}
	
	/*
	 * Check work time
	 */
	if( ovrd && timevec_isdefined(&ovrd->worktime) )
	{
		if( timevec_isnow(&ovrd->worktime, now) )
			good_time = TRUE;
	}
	else if( ovrd && ovrd->sFlags && !nodelist_checkflag(ovrd->sFlags, "CM") )
	{
		/*
		 * It is not an error. We check CM flag only
		 * if the work time was not specified in the override
		 */
		good_time = TRUE;
	}
	else if( node && timevec_isdefined(&node->worktime) )
	{
		if( timevec_isnow(&node->worktime, now) )
			good_time = TRUE;
	}
	
	if( good_time )
	{
		if( good_host )
			return 2;
		else if( good_phone )
			return 1;
	}
	
	return 0;
}

enum { UNHOLD_CALLS, UNHOLD_FREQS };

static void daemon_unhold_system(s_sysentry *syst, int mode)
{
	char abuf[BF_MAXADDRSTR+1];
	s_sess_stat stat_diff;
	
	memset(&stat_diff, '\0', sizeof(s_sess_stat));

	/*
	 * Reset try counters to the TRIES_RESET value
	 */
	session_stat_reset_counters(&stat_diff);
	
	if( mode == UNHOLD_FREQS )
		stat_diff.hold_freqs = HOLD_RESET;
	else
		stat_diff.hold_until = HOLD_RESET;
	
	/*
	 * Write new statistic to the sts file
	 */
	if( session_stat_apply_diff(syst->node.addr, stat_diff) == -1 )
	{
		log("cannot write statistic for address %s",
				ftn_addrstr(abuf, syst->node.addr));
	}

	/*
	 * Read new statistic from sts file, ignore errors
	 */
	(void)session_stat_get(&syst->stat, &syst->node.addr);
}

static bool daemon_node_cancall(s_sysentry *syst, bool ign_wtime, bool tcpip)
{
	char abuf[BF_MAXADDRSTR+1];
	const s_override *ptrl, *lastptr;
	int line;
	int rc;
	
	/* Is this system allready active in an another queue? Skip. */
	if( syst->busy )
		return FALSE;
	
	/* Check for undialable flag */
	if( syst->stat.undialable )
		return FALSE;
	
	if( daemon_branch_exist(syst->node.addr) )
		return FALSE;

	/* Check for netmail presence, when MAILONLY */
	if( conf_options(cf_options) & OPTIONS_MAILONLY )
		if ( !( syst->types & TYPE_NETMAIL ) )
			return FALSE;

	/* Check system hold status */
	if( syst->stat.hold_until > 0 )
	{
		if( time(0) > syst->stat.hold_until )
		{
			log("unholding %s", ftn_addrstr(abuf, syst->node.addr));
			daemon_unhold_system(syst, UNHOLD_CALLS);
		}
		else
			return FALSE;
	}

	/* Check freq hold status */
	if( syst->stat.hold_freqs > 0 )
	{
		if( time(0) > syst->stat.hold_freqs )
		{
			log("unholding freqs for %s", ftn_addrstr(abuf, syst->node.addr));
			daemon_unhold_system(syst, UNHOLD_FREQS);
		}
		else if( !syst->flavors && (syst->types & TYPE_REQUEST) )
			return FALSE;
	}
	
	/* Make checks for all lines */
	if( syst->overrides )
	{
		if( syst->lineptr && syst->lineptr->hidden )
		{
			line = syst->line + 1;
			/* Check lines after current */
			for( ptrl = syst->lineptr->hidden; ptrl; ptrl = ptrl->hidden )
			{
				if( (rc = daemon_node_cancall_line(NULL, ptrl))
				 && (rc == (tcpip ? 2 : 1)) )
				{
					syst->line = line;
					syst->lineptr = ptrl;
					syst->tcpip = (rc == 2) ? TRUE : FALSE;
					return TRUE;
				}
				line++;
			}
		}
		
		if( syst->lineptr == NULL )
			{ lastptr = NULL; syst->line = 0; }
		else
			{ lastptr = syst->lineptr->hidden; }

		line = 0;
		/* Check lines before current */
		for( ptrl = syst->overrides; ptrl && ptrl != lastptr; ptrl = ptrl->hidden )
		{
			if( (rc = daemon_node_cancall_line(line ? NULL : &syst->node, ptrl))
			 && (rc == (tcpip ? 2 : 1)) )
			{
				syst->line = line;
				syst->lineptr = ptrl;
				syst->tcpip = (rc == 2) ? TRUE : FALSE;
				return TRUE;
			}
			line++;
		}
	}
	else /* Node without overriden parameters */
	{
		syst->line = line = 0;
		return daemon_node_cancall_line(&syst->node, NULL);
	}
	
	return FALSE;
}

static int daemon_getnext(const s_sysqueue *q, int current, bool tcpip)
{
	int i;
	int normal = -1;

	if( current < 0 )
		current = 0;

	/* Check systems AFTER the current */
	for( i = current + 1; i < q->sysnum; i++ )
	{
		if( q->systab[i].flavors & FLAVOR_IMMED ) {
			if( daemon_node_cancall(&q->systab[i], TRUE, tcpip) )
				return i;
		} else if( q->systab[i].flavors & FLAVOR_CRASH ) {
			if( daemon_node_cancall(&q->systab[i], FALSE, tcpip) )
				return i;
		} else if( q->systab[i].flavors & FLAVOR_DIRECT
		        || q->systab[i].flavors & FLAVOR_NORMAL ) {
			if( normal == -1 && daemon_node_cancall(&q->systab[i], FALSE, tcpip) )
				normal = i;
		}
	}

	/* Check systems BEFORE the current */
	for( i = 0; (i < current + 1) && (i < q->sysnum); i++ )
	{
		if( q->systab[i].flavors & FLAVOR_IMMED ) {
			if( daemon_node_cancall(&q->systab[i], TRUE, tcpip) )
				return i;
		} else if( q->systab[i].flavors & FLAVOR_CRASH ) {
			if( daemon_node_cancall(&q->systab[i], FALSE, tcpip) )
				return i;
		} else if( q->systab[i].flavors & FLAVOR_DIRECT
		        || q->systab[i].flavors & FLAVOR_NORMAL ) {
			if( normal == -1 && daemon_node_cancall(&q->systab[i], FALSE, tcpip) )
				normal = i;
		}
	}
	
	return normal;
}

static int daemon_getnext2(const s_sysqueue *q, int current, bool tcpip)
{
	int next;

	/* Unlock previous system if available */
	if( current >= 0 )
		q->systab[current].busy = FALSE;

	next = daemon_getnext(q, current, tcpip);
	
	/* Lock new system */
	if( next >= 0 )
		q->systab[next].busy = TRUE;

	return next;
}

static void daemon_do_tries_action(s_sysqueue *q, int pos, s_tries *act, const char *msg)
{
	s_sess_stat stat_diff;
	char abuf[BF_MAXADDRSTR+1];
	
	memset(&stat_diff, '\0', sizeof(s_sess_stat));
	
	if( act->action == TRIES_ACTION_UNDIALABLE )
	{
		log("set %s undialable: %s",
			ftn_addrstr(abuf, q->systab[pos].node.addr), msg);
		stat_diff.undialable = TRUE;
	}
	else if( act->action == TRIES_ACTION_HOLDSYSTEM )
	{
		log("hold %s for %d seconds: %s",
			ftn_addrstr(abuf, q->systab[pos].node.addr), act->arg, msg);
		stat_diff.hold_until = time(NULL) + act->arg;
	}
	else if( act->action == TRIES_ACTION_HOLDALL )
	{
		log("hold all systems for %d seconds: %s", act->arg, msg);
		q->holduntil = time(NULL) + act->arg;
	}

	/*
	 * Write new statistic to the sts file
	 */
	if( session_stat_apply_diff(q->systab[pos].node.addr, stat_diff) == -1 )
	{
		log("cannot write statistic for address %s",
				ftn_addrstr(abuf, q->systab[pos].node.addr));
	}

	/*
	 * Read new statistic from sts file, ignore errors
	 */
	(void)session_stat_get(&q->systab[pos].stat,
			&q->systab[pos].node.addr);
}

static void daemon_process_rc(s_sysqueue *q, s_faddr addr, int rc)
{
	int i;
	s_tries *maxtries        = NULL;
	s_tries *maxtries_noansw = NULL;
	s_tries *maxtries_noconn = NULL;
	s_tries *maxtries_nodial = NULL;
	s_tries *maxtries_hshake = NULL;
	s_tries *maxtries_sessns = NULL;
	char abuf[BF_MAXADDRSTR+1];
	
	for( i = 0; i < q->sysnum; i++ )
		if( ftn_addrcomp(q->systab[i].node.addr, addr) == 0 )
			break;

	if( i < q->sysnum )
	{
		/* Set last call finish time */
		q->systab[i].lastcall = time(0);
	
		/* Temporary set `state' structure to
		   make dynamical configuration work! */
		state.valid  = TRUE;
		state.node   = q->systab[i].node;
		state.listed = q->systab[i].node.listed;
			
		maxtries        = conf_tries(cf_maxtries);
		maxtries_noansw = conf_tries(cf_maxtries_noansw);
		maxtries_noconn = conf_tries(cf_maxtries_noconn);
		maxtries_nodial = conf_tries(cf_maxtries_nodial);
		maxtries_hshake = conf_tries(cf_maxtries_hshake);
		maxtries_sessns = conf_tries(cf_maxtries_sessions);
			
		/* Reset `state' structure */
		init_state(&state);
			
		/* Update statistic, ignore errors */
		session_stat_get(&q->systab[i].stat, &addr);
			
		log("%s: rc = %d [%d/%d/%d/%d/%d/%d]",
			ftn_addrstr(abuf, addr), rc,
			q->systab[i].stat.tries,
			q->systab[i].stat.tries_noansw,
			q->systab[i].stat.tries_noconn,
			q->systab[i].stat.tries_nodial,
			q->systab[i].stat.tries_hshake,
			q->systab[i].stat.tries_sessns);
			
		DEB((D_DAEMON, "daemon_process_rc: address %s: [%d/%d/%d/%d/%d/%d]",
			ftn_addrstr(abuf, addr),
			q->systab[i].stat.tries,
			q->systab[i].stat.tries_noansw,
			q->systab[i].stat.tries_noconn,
			q->systab[i].stat.tries_nodial,
			q->systab[i].stat.tries_hshake,
			q->systab[i].stat.tries_sessns));
			
		if( maxtries && q->systab[i].stat.tries >= maxtries->tries )
		{
			daemon_do_tries_action(q, i, maxtries, "reached maximal tries count");
		}
		else if( maxtries_noansw && q->systab[i].stat.tries_noansw >= maxtries_noansw->tries )
		{
			daemon_do_tries_action(q, i, maxtries_noansw, "reached maximal no answers count");
		}
		else if( maxtries_noconn && q->systab[i].stat.tries_noconn >= maxtries_noconn->tries )
		{
			daemon_do_tries_action(q, i, maxtries_noconn, "reached maximal connect failures count");
		}
		else if( maxtries_nodial && q->systab[i].stat.tries_nodial >= maxtries_nodial->tries )
		{
			daemon_do_tries_action(q, i, maxtries_nodial, "reached maximal no dialtone count");
		}
		else if( maxtries_hshake && q->systab[i].stat.tries_hshake >= maxtries_hshake->tries )
		{
			daemon_do_tries_action(q, i, maxtries_hshake, "reached maximal handshake failures count");
		}
		else if( maxtries_sessns && q->systab[i].stat.tries_sessns >= maxtries_sessns->tries )
		{
			daemon_do_tries_action(q, i, maxtries_sessns, "reached maximal session failures count");
		}
	}
	else
	{
		/* No entry in the systems queue
		 * TODO: load statistic and check tries? */
		log("%s: rc = %d",
			ftn_addrstr(abuf, addr), rc);
		DEB((D_DAEMON, "daemon_process_rc: address %s: [%d/%d/%d/%d/%d]",
			ftn_addrstr(abuf, addr)));
	}
}

static void daemon_alive_message(s_sysqueue *q)
{
	int i, holded_num = 0;
	
	for( i = 0; i < q->sysnum; i++ )
		if( q->systab[i].stat.hold_until
		 || q->systab[i].stat.hold_freqs )
			holded_num++;
	
	log("still alive (%d systems, %d holded, %d branches)",
		q->sysnum, holded_num, daemon_branch_number());
}

static void daemon_queue_do(s_daemon_queue *dq)
{
	bool select_next_address = FALSE;
	s_sysqueue *q = dq->q;
#ifdef DEBUG
	char abuf[BF_MAXADDRSTR+1];
#endif
	
	if( dq->current >= 0 )
	{
		if( (q->systab[dq->current].lastcall == 0)
		 || (q->systab[dq->current].lastcall + dq->circle < time(0)) )
		{
			if( !daemon_branch_exist(q->systab[dq->current].node.addr) )
			{
				DEB((D_DAEMON, "daemon_queue: calling %s",
					ftn_addrstr(abuf, q->systab[dq->current].node.addr)));
				if( !daemon_call(&q->systab[dq->current]) )
					select_next_address = TRUE;
			}
			else
				select_next_address = TRUE;
		}
	}
	else
		select_next_address = TRUE;

	if( select_next_address )
	{
		int next = daemon_getnext2(q, dq->current, dq->tcpip);
		DEB((D_DAEMON, "daemon_queue: next = %d, current = %d",
			next, dq->current));
		
		if( next >= 0 )
		{
			dq->current = next;

			/*
			 * Temporary set `state' structure to
			 * make dynamical configuration work!
			 */
			state.valid  = TRUE;
			state.node   = q->systab[dq->current].node;
			state.listed = q->systab[dq->current].node.listed;
			state.inet   = dq->tcpip;
			
			if( (q->systab[dq->current].flavors & FLAVOR_IMMED) )
				dq->circle = conf_number(cf_daemon_circle_immed);
			else if( (q->systab[dq->current].flavors & FLAVOR_CRASH) )
				dq->circle = conf_number(cf_daemon_circle_crash);
			else if( (q->systab[dq->current].flavors & FLAVOR_DIRECT) )
				dq->circle = conf_number(cf_daemon_circle_direct);
			else
				dq->circle = conf_number(cf_daemon_circle_normal);
			
			init_state(&state);
		}
		else
			dq->current = -1;
	}
}

void daemon_queues_update_current(s_daemon_queue dqs[], int pos)
{
	int i;

	for( i = 0; dqs[i].q; i++ )
	{
		if( dqs[i].current < 0 )
			continue;
		
		if( dqs[i].current == pos )
			dqs[i].current = -1;
		else if( dqs[i].current > pos )
			dqs[i].current--;
	}
}

int daemon_rescan_sysqueue(s_sysqueue *q, s_daemon_queue dqs[])
{
	int i;
	s_outbound_callback_data ocb;
	
	out_reset_sysqueue(q);
	
	memset(&ocb, '\0', sizeof(s_outbound_callback_data));
	ocb.callback = out_handle_sysqueue;
	ocb.dest = (void *)q;
	if( out_scan(&ocb, NULL) )
	{
		log("error scanning outbound");
		return -1;
	}

	/*
	 * Remove empty entries from sysqueue (e.g. the mail was
	 * removed by the sysop or by an another program)
	 */
	for( i = 0; i < q->sysnum; i++ )
		if( !q->systab[i].types && !q->systab[i].flavors )
		{
			out_remove_from_sysqueue(q, i);
			daemon_queues_update_current(dqs, i);
		}
	
#ifdef DEBUG
	log_sysqueue(q);
#endif

	return 0;
}

int daemon_remove_waiting_branches(s_sysqueue *q, s_daemon_queue dqs[])
{
	int i;
	int j;
	s_daemon_branch *bptr;
#ifdef DEBUG
	char abuf[BF_MAXADDRSTR+1];
#endif
	
	while( (i = daemon_branch_get_first_waiting()) >= 0 )
	{
		bptr = daemon_branch_get_pointer(i);
		if( !bptr )
		{
			log("internal error: cannot get branch pointer!");
			return -1;
		}
		
		DEB((D_DAEMON, "daemon: process branch information for %s, rc = %d",
			ftn_addrstr(abuf, bptr->addr), bptr->rc));
		
		if( bptr->tcpip )
			--tcpip_clients;
		else
			--modem_clients;
		
		daemon_process_rc(q, bptr->addr, bptr->rc);
		
		if( bptr->rc == BFERR_NOERROR )
		{
			/* Remove system from queue */
			for( j = 0; j < q->sysnum; j++ )
				if( !ftn_addrcomp(bptr->addr, q->systab[j].node.addr) )
				{
					out_remove_from_sysqueue(q, j);
					daemon_queues_update_current(dqs, j);
				}	
		}
	
		/* Hold modem line for some time */
		if( !bptr->tcpip && bptr->portname )
		{
			daemon_line_hold(bptr->portname,
				conf_number(cf_daemon_circle_modem));
		}
		
		/* Remove branch entry */
		daemon_branch_remove(i);
	}

	return 0;
}

#define	PIDFILE_CREATE	1
#define PIDFILE_DELETE	2
#define PIDFILE_TERMINATE 3

int daemon_pidfile(int cmd)
{
	char *pidfile	    = conf_string(cf_daemon_pid_file);
	pid_t hispid, mypid = getpid();
	FILE *pf;
	struct stat sb;

	if( !pidfile )
		return 0;
	
	switch (cmd) {
		case PIDFILE_CREATE:
			if( !stat(pidfile, &sb) ) {
				pf = fopen(pidfile, "r");
				if ( !pf ) {
					log("daemon_pidfile: cannot open %s for reading", pidfile);
					return -1;
				}	
				
				fscanf(pf, "%d", &hispid);
				fclose(pf);

				if( hispid ) {
					if( hispid == mypid )
						return 0;
					if( !kill(hispid, 0) ) {
						log("daemon_pidfile: another daemon exist. pid=%d", hispid);
						return -1;
					} 
					if( errno != ESRCH ) {
						log("daemon_pidfile: error sending signal. pid=%d, errno=%d", hispid, errno);
						return -1;
					}	
				}	
			} else if( errno != ENOENT )
			{
				log("daemon_pidfile: error stat(%s). errno=%d", pidfile, errno);
				return -1;
			}	
			
			pf = fopen(pidfile, "w");
			if( !pf ) {
				log("daemon_pidfile: cannot open %s for writing", pidfile);
				return -1;
			}	
			
			fprintf(pf, "%d", mypid);
			fclose(pf);
			break;
			
		case PIDFILE_DELETE:
			if( !stat(pidfile, &sb) ) {
				pf = fopen(pidfile, "r");
				if( !pf ) {
					log("daemon_pidfile: cannot open %s for reading", pidfile);
					return -1;
				}
				
				fscanf(pf, "%d", &hispid);
				fclose(pf);
				
				if( !hispid  || (hispid == mypid) ) {
					unlink(pidfile);
					return 0;
				} else {
					log("daemon_pidfile: oops! mypid=%d, hispid=%d", mypid, hispid);
					return -1;
				}
			} else {
				log("daemon_pidfile: error stat(%s). errno=%d", pidfile, errno);
				return -1;
			}
			break;
		
		case PIDFILE_TERMINATE:
			if( !stat(pidfile, &sb) ) {
				pf = fopen(pidfile, "r");
				if( !pf ) {
					log("daemon_pidfile: cannot open %s for reading", pidfile);
					return -1;
				}
				
				fscanf(pf, "%d", &hispid);
				fclose(pf);
				unlink(pidfile);
				
				if( hispid )
					kill(hispid, 15);

			} else {
				log("daemon_pidfile: error stat(%s). errno=%d", pidfile, errno);
				return -1;
			}
			break;
		
		default:
			log("daemon_pidfile: undefined cmd = %d", cmd);
			return -1;
	}

	return 0;
}

int daemon_run(const char *confname, const char *incname, bool quit)
{
	DM_State dmstate    = DM_Start;
	time_t timer_rescan = 0;
	time_t timer_alive  = 0;
	bool started        = FALSE;
	int circle_rescan   = 0;
	int rc              = 0;
	int i;

	if ( quit ) {
		daemon_pidfile(PIDFILE_TERMINATE);
		exit(0);
	}
	
	/* Initialisation */
	memset(&daemon_sys_queue, '\0', sizeof(s_sysqueue));
	
	/* Install signal handlers */
	signal(SIGCHLD, daemon_sighandler_chld);
	signal(SIGINT,  daemon_sighandler);
	signal(SIGTERM, daemon_sighandler);
	signal(SIGHUP,  daemon_sighandler);
	signal(SIGUSR1, daemon_sighandler);
	signal(SIGUSR2, daemon_sighandler);
	
	while(1)
	{
		switch(dmstate) {
		case DM_Start:
			DEB((D_DAEMON, "daemon: entering state DM_Start"));
			
			if ( daemon_pidfile(PIDFILE_CREATE) == -1 )
				exit(0);
			
			circle_rescan = conf_number(cf_daemon_circle_rescan);
			max_tcpip     = conf_number(cf_daemon_maxclients_tcpip);
			max_modem     = conf_number(cf_daemon_maxclients_modem);
			
			/* Set default values */
			if( !circle_rescan ) circle_rescan = 60;
						
			/* Disable counting of files/mail size */
			sysqueue_dont_count_sizes = TRUE;
			
			/* Setup sysqueue scanner callbacks */
			sysqueue_add_callback = daemon_sysentry_init;
			sysqueue_rem_callback = daemon_sysentry_deinit;

			/* Reopen log and debug files */
			if( log_isopened() )
				log_close();
#ifdef DEBUG
			if( debug_isopened() )
				debug_close();
#endif
			if( log_open(log_getfilename(LOG_FILE_DAEMON), NULL, NULL) )
			{
				log("can't continue without logging");
				return BFERR_FATALERROR;
			}

			log("%sstarting daemon (%s)",
				started ? "re" : "", BF_VERSION);

			started = TRUE;
			dmstate = DM_ProcessQueues;
			break;

		case DM_Restart:
			DEB((D_DAEMON, "daemon: entering state DM_Restart"));

			/* Release memory */
			deinit_sysqueue(&daemon_sys_queue);
			deinit_conf();

			/* Reset queues */
			daemon_queues[MODEM_QUEUE].current =
				daemon_queues[TCPIP_QUEUE].current = -1;
			daemon_queues[MODEM_QUEUE].circle =
				daemon_queues[TCPIP_QUEUE].circle = 0;
			daemon_queues[MODEM_QUEUE].state =
				daemon_queues[TCPIP_QUEUE].state = DQ_Idle;
			
			/* Read primary config file */
			if( confname && *confname )
				rc = conf_readconf(confname, 0);
			else
				rc = conf_readconf(conf_getconfname(), 0);
			
			if( rc )
				return(BFERR_FATALERROR);
			
			/* Read additional config file (manual include) */
			if( incname && *incname && conf_readconf(incname, 1) )
				log("cannot read additional config (ignore)");
			
			dmstate = DM_Start;
			break;

		case DM_ProcessQueues:
			DEB((D_DAEMON, "daemon: entering state DM_ProcessQueues"));
			
			/*
			 * Handle received signals
			 */
			for( i = 0; daemon_signals[i].signum; i++ )
				if( daemon_signals[i].wait )
					break;

			if( daemon_signals[i].signum )
			{
				daemon_signals[i].wait = FALSE;
				dmstate = daemon_signals[i].state;
				break;
			}

			/*
			 * Process finished branches
			 */
			(void)daemon_remove_waiting_branches(&daemon_sys_queue,
					daemon_queues);
			
			/*
			 * Check rescan timer
			 */
			if( !timer_running(timer_rescan) || timer_expired(timer_rescan) )
			{
				(void)daemon_rescan_sysqueue(&daemon_sys_queue,
						daemon_queues);
				timer_set(&timer_rescan, circle_rescan);
			}
			
			/*
			 * Check alive timer
			 */
			if( !timer_running(timer_alive) || timer_expired(timer_alive) )
			{
				daemon_alive_message(&daemon_sys_queue);
				timer_set(&timer_alive, DAEMON_ALIVE_TIMER);
			}
			
			if( max_modem > 0 )
				daemon_queue_do(&daemon_queues[MODEM_QUEUE]);
			if( max_tcpip > 0 )
				daemon_queue_do(&daemon_queues[TCPIP_QUEUE]);
			
			(void)sleep(DAEMON_IDLE_SLEEP);
			break;

		case DM_Usr1:
			DEB((D_DAEMON, "daemon: entering state DM_Usr1"));

			log("signal USR1 has no effect now. Have any ideas?");
			dmstate = DM_ProcessQueues;
			
			break;
		
		case DM_Usr2:
			DEB((D_DAEMON, "daemon: entering state DM_Usr2"));

			log("signal USR2 has no effect now. Have any ideas?");
			dmstate = DM_ProcessQueues;
			
			break;
			
		case DM_Shutdown:
			DEB((D_DAEMON, "daemon: entering state DM_Shutdown"));
			
			log("stopping daemon");
			daemon_pidfile(PIDFILE_DELETE);
			
			/* Release memory */
			deinit_sysqueue(&daemon_sys_queue);
			daemon_branch_deinit();
			daemon_lines_deinit();
			deinit_conf();
			
			exit(0);
		}
	}
}

