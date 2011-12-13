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
#include "bforce.h"
#include "nodelist.h"
#include "session.h"

/* PID of our child process (if use fork) */
pid_t child_pid = 0;

const char *BFERR[] = {
	/* 00 */ "Successfull",
	/* 01 */ "Fatal error occured",
	/* 02 */ "Unknown phone number(or ip address)",
	/* 03 */ "Modem port busy (or open error)",
	/* 04 */ "System locked now",
	/* 05 */ "Try later",
	/* 06 */ "Modem not response",
	/* 07 */ "Not working now",
	/* 08 */ "Unused entry",
	/* 09 */ "Unused entry",
	/* 10 */ "Can't connect to remote",
	/* 11 */ "Can't connect to remote: busy",
	/* 12 */ "Can't connect to remote: nocarrier",
	/* 13 */ "Can't connect to remote: nodialtone",	
	/* 14 */ "Can't connect to remote: noanswer",
	/* 15 */ "Can't connect to remote: error",
	/* 16 */ "Can't connect to remote: user defined 16",
	/* 17 */ "Can't connect to remote: user defined 17",
	/* 18 */ "Can't connect to remote: user defined 18",
	/* 19 */ "Can't connect to remote: user defined 19",
	/* 20 */ "Connect speed too low",
	/* 21 */ "Cannot handshake with remote",
	/* 22 */ "Xmiting error",
	/* 23 */ "CPS too low",
	/* 24 */ "Reached stop time",
	/* 25 */ "Unused entry",
	/* 26 */ "Unused entry",
	/* 27 */ "Unused entry"
};

static void deinit_opts(s_bforce_opts *opts);

/*
static void print_compiled_configuration(void)
{
	printf("BFORCE_LOGFILE = "BFORCE_LOGFILE"\n");
	printf("BFORCE_DEBFILE = "BFORCE_DEBFILE"\n");
	printf("DAEMON_LOGFILE = "DAEMON_LOGFILE"\n");
	printf("BFORCE_CFGFILE = "BFORCE_CFGFILE"\n");
	printf("BF_OS = "BF_OS"\n");
}
*/

static void usage(void)
{
	printf_usage(NULL,
		"usage: bforce [-fmh] [-I<include>] [-n<phone>] [-l<line_number>]\n"
		"              [-a<ip_address>] [-S<connect>] [-p<device>] <node>\n"
		"       bforce [-ih] [-I<include>] [-S<connect>]\n"
		"              <tsync|yoohoo|emsi|binkp|auto> (this implies slave mode)\n"
		"       bforce [-dh] [-C<config>] [-I<include>]\n"
		"\n"
		"options:\n"
		"  -d                run as daemon\n"
		"  -q                terminate daemon\n"
		"  -i                run from inetd (for slave mode only)\n"
		"  -f                ignore system's work time\n"
		"  -o                starts outgoing session on stdin/stdout\n"
		"  -C <config>       main config file name (\"%s\")\n"
		"  -I <config>       additional config file name\n"
		"  -n <phone>        override phone number\n"
		"  -l <line_number>  call on this hidden line (default is 0) \n"
		"  -a <ip_address>   override internet address\n"
		"  -S <connect_str>  connect string (for slave mode only)\n"
		"  -p <port>         override modem port (must be defined in config)\n"
		"  -h                show this help message\n"
		"\n",
		conf_getconfname()
	);
}

int bforce_create_dirs(void)
{
	const char *p_dirname = conf_string(cf_status_directory);

	if( p_dirname )
	{
		if( access(p_dirname, F_OK) == -1 || !is_directory(p_dirname) )
		{
			if( directory_create(p_dirname, 0700) == -1 )
			{
				logerr("cannot create spool directory \"%s\"", p_dirname);
				return -1;
			}
		}
	}

	return 0;
}

/*
 *  Universal signal handler for parent processes :) Will resend
 *  signals to child process
 */
static RETSIGTYPE parent_sighandler(int sig)
{
	/* Send this signal to our child */
	if( child_pid ) kill(child_pid, sig);
}

/*
 *  Return non-zero value to indicate error, on success - return zero
 *  to child process and NEVER return to parent process
 */
static int usefork(void)
{
	pid_t cpid;	/* child PID */
	pid_t wpid;	/* waitpid() returned value */
	int status = 0;

	cpid = fork();
	
	if( cpid == 0 )
		{ return 0; }

	if( cpid < 0 )
		{ logerr("failed fork() call"); return 1; }
		
	/*
	 *  Now we are parent process
	 */

	/* Put PID into global variable */
	child_pid = cpid;
	
	/* Setup signals handling */
	signal(SIGHUP,  parent_sighandler);
	signal(SIGINT,  parent_sighandler);
	signal(SIGTERM, parent_sighandler);
	signal(SIGUSR1, parent_sighandler);
	signal(SIGUSR2, parent_sighandler);
	
	/* Wait until child die */
	while( (wpid = waitpid(cpid, &status, 0)) < 0 && errno == EINTR )
	{
		/* EMPTY LOOP */
	}
	
	if( wpid == cpid && WIFEXITED(status) )
	{
		/* Child was terminated with _exit() */
		exit(WEXITSTATUS(status));
	}
	else if( wpid < 0 && WIFSIGNALED(status) )
	{
		/* Child was terminated by signal */
		kill(getpid(), WTERMSIG(status));
	}
	exit(BFERR_FATALERROR);
}

static int bforce_master(const s_bforce_opts *opts)
{
	s_falist *tmpl;
	int rc, maxrc = BFERR_NOERROR;
		
#ifdef GETPGRP_VOID
	if( getpgrp() == getpid() )
#else
	if( getpgrp(0) == getpid() )
#endif
	{
		/* We are process group leader -> fork() */
		if( usefork() ) return BFERR_FATALERROR;
	}
		
	for( tmpl = opts->addrlist; tmpl; tmpl = tmpl->next )
	{
		int callopt = 0;
		
		if( opts->iaddr ) callopt |= CALLOPT_INET;
		if( opts->force ) callopt |= CALLOPT_FORCE;
		
		rc = call_system(tmpl->addr, opts);
		
		if( rc > maxrc ) maxrc = rc;
	}

	return maxrc;
}

static int bforce_slave(const s_bforce_opts *opts)
{
	return answ_system(opts->stype, opts->connect, opts->inetd);
}

static int bforce_daemon(const s_bforce_opts *opts)
{
	int forkrc = fork();

	if( forkrc == -1 )
        {
		logerr("cannot run daemon: failed fork() call");
		return BFERR_FATALERROR;
	}
	else if( forkrc > 0 )
	{
		return BFERR_NOERROR;
	}

	/*
	 * We are inside a child process.
	 * Create new session and run daemon
	 */
	setsid();
	
	return daemon_run(opts->confname, opts->incname, opts->quit);
}
				
int main(int argc, char *argv[], char *envp[])
{
	s_bforce_opts opts;
	int rc = 0;
	int ch = 0;
	int role = 0;
	
	memset(&opts, '\0', sizeof(s_bforce_opts));
	
	while( (ch=getopt(argc, argv, "hodqr:ifC:I:n:l:a:S:p:")) != EOF )
	{
		switch( ch ) {
		case 'h':
			usage();
			exit(BFERR_NOERROR);
		case 'd':
			if( opts.inetd || opts.force || opts.phone
			 || opts.hiddline || opts.iaddr || opts.connect
			 || opts.device || opts.quit )
				{ usage(); exit(BFERR_FATALERROR); }
			else
				{ opts.daemon = 1; }
			break;
		case 'q':
			if( opts.inetd || opts.force || opts.phone
			 || opts.hiddline || opts.iaddr || opts.connect
			 || opts.device || opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
				{ opts.daemon = 1; opts.quit = 1; }
			break;
		case 'i':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
				{ opts.inetd = 1; }
			break;
		case 'f':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
				{ opts.force = 1; }
			break;
		case 'o':
			if( opts.dontcall )
				{ usage(); exit(BFERR_FATALERROR); }
			else
				{ opts.dontcall = TRUE; }
			break;
		case 'C':
			if( opts.confname ) free(opts.confname);
			if( optarg ) opts.confname = (char *)xstrcpy(optarg);
			break;
		case 'I':
			if( opts.incname ) free(opts.incname);
			if( optarg ) opts.incname = (char *)xstrcpy(optarg);
			break;
		case 'n':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
			{
				if( opts.phone ) free(opts.phone);
				if( optarg ) opts.phone = (char *)xstrcpy(optarg);
			}
			break;
		case 'l':
			if( ISDEC(optarg) && opts.daemon == 0 )
				opts.hiddline = atoi(optarg);
			else
				{ usage(); exit(BFERR_FATALERROR); }
			break;
		case 'a':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
			{
				if( opts.iaddr ) free(opts.iaddr);
				if( optarg ) opts.iaddr = (char *)xstrcpy(optarg);
			}
			break;
		case 'S':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
			{
				if( opts.connect ) free(opts.connect);
				if( optarg ) opts.connect = (char *)xstrcpy(optarg);
			}
			break;
		case 'p':
			if( opts.daemon )
				{ usage(); exit(BFERR_FATALERROR); }
			else
			{
				if( opts.device ) free(opts.device);
				if( optarg ) opts.device = (char *)xstrcpy(optarg);
			}
			break;
		default :
			usage();
			exit(BFERR_FATALERROR);
		}
	}
	
	/* Expression checker use it, so init first */
	init_state(&state);
	
	/* Set space available for process title */
#ifndef HAVE_SETPROCTITLE
	setargspace(argv, envp);
#endif
	
	/* Initialise random number generation */
	(void)srand((unsigned)time(NULL));
	
	/* Initialise current locale */
	(void)setlocale(LC_ALL, "");
	
	/* Set secure process umask */
	(void)umask(~(S_IRUSR|S_IWUSR));
	
	/* Now process non-option arguments */
	if( opts.daemon == FALSE )
	{
		const char *p;
		s_faddr addr;
		s_falist **alist = &opts.addrlist;
		
		while( (optind < argc) && (p = argv[optind++]) )
		{
			for( ; *p == '*'; p++ );
		
			if( strcasecmp(p, "tsync") == 0 )
			{
				role = 0;
				opts.stype = SESSION_FTSC;
			}
			else if( strcasecmp(p, "yoohoo") == 0 )
			{
				role = 0;
				opts.stype = SESSION_YOOHOO;
			}
			else if( strcasecmp(p, "**EMSI_INQC816") == 0 )
			{
				role = 0;
				opts.stype = SESSION_EMSI;
			}
			else if( strncasecmp(p, "emsi", 4) == 0 )
			{
				role = 0;
				opts.stype = SESSION_EMSI;
			}
			else if( strcasecmp(p, "binkp") == 0 )
			{
				role = 0;
				opts.stype = SESSION_BINKP;
			}
			else if( strcasecmp(p, "auto") == 0 )
			{
				role = 0;
				opts.stype = SESSION_UNKNOWN;
			}
			else if( ftn_addrparse(&addr, p, FALSE) == 0 )
			{
				role = 1;
				(*alist) = (s_falist*)xmalloc(sizeof(s_falist));
				memset(*alist, '\0', sizeof(s_falist));
				(*alist)->addr = addr;
				alist = &(*alist)->next;
			}
			else
			{
				printf("invalid address \"%s\"", p);
				usage();
				exit(BFERR_FATALERROR);
			}
		}

		if( opts.dontcall && role == 0 )
		{
			usage();
			exit(BFERR_FATALERROR);
		}
	}
	
/*	if( (rc = log_open(log_getfilename(LOG_FILE_SESSION), NULL, NULL)) )
	{
		log("can't continue without logging");
		gotoexit(BFERR_FATALERROR);
	}
*/	
	/* Process primary config file */
	if( opts.confname && *opts.confname )
		rc = conf_readconf(opts.confname, 0);
	else
		rc = conf_readconf(conf_getconfname(), 0);
	
	if( rc ) gotoexit(BFERR_FATALERROR);
	
	/* Process additional config file, ignore errors */
	if( opts.incname && *opts.incname )
		(void)conf_readconf(opts.incname, 1);
	
	/* Reopen log file if it was defined in config */
	if( log_open(log_getfilename(LOG_FILE_SESSION), NULL, NULL) )
	{
		log("can't continue without logging");
		gotoexit(BFERR_FATALERROR);
	}
	
#ifdef DEBUG
	/* Same for the debug file */
	(void)debug_setfilename(log_getfilename(LOG_FILE_DEBUG));
#endif
	
	printf("split_inb: %s\n", conf_boolean(cf_split_inbound)?"yes":"no");
	
	if( opts.daemon )
		rc = bforce_daemon(&opts);
	else if( role )
		rc = bforce_master(&opts);
	else
		rc = bforce_slave(&opts);
	
exit:
	
	deinit_conf();
	deinit_opts(&opts);
	
	/* Shutdown logging services */
	if( log_isopened() ) log_close();
#ifdef DEBUG
	if( debug_isopened() ) debug_close();
#endif

	exit(rc);
}

static void deinit_opts(s_bforce_opts *opts)
{
	if( opts->confname ) free(opts->confname);
	if( opts->incname  ) free(opts->incname);
	if( opts->phone    ) free(opts->phone);
	if( opts->iaddr    ) free(opts->iaddr);
	if( opts->connect  ) free(opts->connect);
	if( opts->device   ) free(opts->device);
	if( opts->addrlist ) deinit_falist(opts->addrlist);
	
	memset(opts, '\0', sizeof(s_bforce_opts));
}

