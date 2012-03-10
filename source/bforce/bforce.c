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
		"call:\n"
		"  use nodelist and config overrides\n"
		"    bforce [-f] [-I<include>] [-p<device>] <node>\n"
		"  use modem\n"
		"    bforce [-f] [-I<include>] -n<phone> [-l<line_number>] \n"
		"              [-p<device>] <node>\n"
		"  use TCP/IP\n"
		"    bforce [-I<include>] [-a<ip_address>] -u proto <node>\n"
		"  start on stdio\n"
		"    bforce [-f] [-I<include>] -o <node>\n"
		"\n"
		"answer:\n"
		"  bforce [-i] [-I<include>] [-S<connect>]\n"
		"     <tsync|yoohoo|emsi|binkp|auto>\n"
		"\n"
		"start daemon:\n"
		"  bforce -d [-C<config>] [-I<include>]\n"
		"\n"
		"stop daemon:\n"
		"  bforce -q [-C<config>] [-I<include>]\n"
		"\n"
		"options:\n"
		"  -i                      run from inetd (for slave mode only)\n"
		"  -f                      ignore system's work time\n"
		"  -C <config>             main config file name (\"%s\")\n"
		"  -I <config>             additional config file name (one allowed)\n"
		"  -n <phone>              override phone number\n"
		"  -l <line_number>        call on this hidden line (default is 0) \n"
		"  -a <ip_address>         override internet address\n"
		"  -S <connect_str>        connect string (for slave mode only)\n"
		"  -p <port>               override modem port (must be defined in config)\n"
		"  -u binkp|ifcico|telnet  protocol to use over TCP/IP\n"
		"  -h                      show this help message\n"
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
		
		if( opts->runmode == MODE_CALL_IP ) callopt |= CALLOPT_INET;
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
	opts.runmode = MODE_UNDEFINED;
	
	memset(&opts, '\0', sizeof(s_bforce_opts));
	
	// parsing
	
	while( (ch=getopt(argc, argv, "hfI:p:n:l:a:u:oiC:S:dq")) != EOF )
	{
		switch( ch ) {
		case 'h':
			usage();
			exit(BFERR_NOERROR);
		case 'f':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_MODEM;
			if( opts.runmode != MODE_CALL_MODEM || opts.force ) { usage(); exit(BFERR_FATALERROR); }
			opts.force = 1;
			break;
		case 'I':
			if( opts.incname || !optarg ) { usage(); exit(BFERR_FATALERROR); }  //free(opts.incname);
			opts.incname = (char *)xstrcpy(optarg);
			break;
		case 'p':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_MODEM;
			if( opts.runmode != MODE_CALL_MODEM || opts.device || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			opts.device = (char *)xstrcpy(optarg);
			break;
		case 'n':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_MODEM;
			if( opts.runmode != MODE_CALL_MODEM || opts.phone || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			//if( opts.phone ) free(opts.phone);
			opts.phone = (char *)xstrcpy(optarg);
			break;
		case 'l':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_MODEM;
			if( opts.runmode != MODE_CALL_MODEM || opts.hiddline || !optarg || ISDEC(optarg) ) { usage(); exit(BFERR_FATALERROR); }
			opts.hiddline = atoi(optarg);
			break;
		case 'a':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_IP;
			if( opts.runmode != MODE_CALL_IP || opts.iphost || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			opts.iphost = (char *)xstrcpy(optarg);
			break;
		case 'u':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_IP;
			if( opts.runmode != MODE_CALL_IP || opts.ipproto || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			opts.ipproto = (char *)xstrcpy(optarg);
			break;
		case 'o':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_STDIO;
			if( opts.runmode != MODE_CALL_STDIO || opts.usestdio ) { usage(); exit(BFERR_FATALERROR); }
			opts.usestdio = TRUE;
			break;
		case 'i':
			//if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
			//if( opts.runmode != MODE_ANSWER || opts.inetd ) { usage(); exit(BFERR_FATALERROR); }
			opts.inetd = 1;
			break;
		case 'C':
			if( opts.confname || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			opts.confname = (char *)xstrcpy(optarg);
			break;
		case 'S':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
			if( opts.runmode != MODE_ANSWER || opts.connect || !optarg ) { usage(); exit(BFERR_FATALERROR); }
			opts.connect = (char *)xstrcpy(optarg);
			break;
		case 'd':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_DAEMON;
			if( opts.runmode != MODE_DAEMON || opts.daemon || opts.quit ) { usage(); exit(BFERR_FATALERROR); }
			opts.daemon = 1;
			break;
		case 'q':
			if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_DAEMON;
			if( opts.runmode != MODE_DAEMON || opts.quit ) { usage(); exit(BFERR_FATALERROR); }
			opts.quit = 1;
			opts.daemon = 1;
			break;
		default :
			usage();
			exit(BFERR_FATALERROR);
		}
	}
	
	if( opts.inetd && opts.runmode != MODE_ANSWER && opts.runmode != MODE_CALL_STDIO )
	{
		usage();
		exit(BFERR_FATALERROR);
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
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_FTSC;
			}
			else if( strcasecmp(p, "yoohoo") == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_YOOHOO;
			}
			else if( strcasecmp(p, "**EMSI_INQC816") == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_EMSI;
			}
			else if( strncasecmp(p, "emsi", 4) == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_EMSI;
			}
			else if( strcasecmp(p, "binkp") == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_BINKP;
			}
			else if( strcasecmp(p, "auto") == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_ANSWER;
				if( opts.runmode != MODE_ANSWER || opts.stype ) { usage(); exit(BFERR_FATALERROR); }
				opts.stype = SESSION_UNKNOWN;
			}
			else if( ftn_addrparse(&addr, p, FALSE) == 0 )
			{
				if( opts.runmode == MODE_UNDEFINED ) opts.runmode = MODE_CALL_DEFAULT;
				if( opts.runmode != MODE_CALL_DEFAULT && opts.runmode != MODE_CALL_IP &&
				    opts.runmode != MODE_CALL_MODEM && opts.runmode != MODE_CALL_STDIO ) { usage(); exit(BFERR_FATALERROR); }
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
	}

/*	if( (rc = log_open(BFORCE_LOGFILE, NULL, NULL)) ) //compiled in
	{
		log("can't continue without logging");
		gotoexit(BFERR_FATALERROR);
	}*/
	
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
	if( log_reopen(log_getfilename(LOG_FILE_SESSION), NULL, NULL) )
	{
		log("can't continue without logging");
		gotoexit(BFERR_FATALERROR);
	}
	
	//char runmode_str[21];
	//snprintf(runmode_str, 20, "Run mode: %d", opts.runmode);
	//log(runmode_str);
	
	switch( opts.runmode )
	{
case MODE_DAEMON:
		log("Daemon mode");
		rc = bforce_daemon(&opts);
		break;
case MODE_CALL_DEFAULT:
case MODE_CALL_IP:
case MODE_CALL_MODEM:
case MODE_CALL_STDIO:
		log("Outgoing call");
		rc = bforce_master(&opts);
		break;
case MODE_ANSWER:
		log("Start answer");
		rc = bforce_slave(&opts);
		break;
default:
		log("Could not determine run mode");
	}

exit:
	
	deinit_conf();
	deinit_opts(&opts);
	
	/* Shutdown logging services */
	if( log_isopened() ) log_close();
#ifdef DEBUG
	if( debug_isopened() ) debug_close();
#endif
        DEB((D_FREE, "good exit"));
	exit(rc);
}

static void deinit_opts(s_bforce_opts *opts)
{
	if( opts->confname ) free(opts->confname);
	if( opts->incname  ) free(opts->incname);
	if( opts->phone    ) free(opts->phone);
	if( opts->iphost   ) free(opts->iphost);
	if( opts->ipproto  ) free(opts->ipproto);
	if( opts->connect  ) free(opts->connect);
	if( opts->device   ) free(opts->device);
	if( opts->addrlist ) deinit_falist(opts->addrlist);
	
	memset(opts, '\0', sizeof(s_bforce_opts));
}

