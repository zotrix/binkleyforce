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

struct {
	char *name;
	int value;
} exec_options_names[] = {
	{ "nowait",    EXEC_OPT_NOWAIT   },
	{ "setsid",    EXEC_OPT_SETSID   },
	{ "logout",    EXEC_OPT_LOGOUT   },
	{ "useshell",  EXEC_OPT_USESHELL },
	{ NULL,        0                 }
};

int exec_file_exist(const char *command)
{
	const char *p;
	
	ASSERT(command);
	
	if( *command == '[' )
	{
		p = strchr(command + 1, ']');
		if( !p )
			return -1;
		++p;
	}
	else
		p = command;

	if( !access(p, X_OK) )
		return 0;

	return -1;
}

int exec_options_parse(char *str)
{
	int yield = 0;
	char *p;
	char *n;
	int i;

	for( p = string_token(str, &n, ", \t", 0); p;
	     p = string_token(NULL, &n, ", \t", 0) )
	{
		for( i = 0; exec_options_names[i].name; i++ )
			if( !strcasecmp(p, exec_options_names[i].name) )
			{
				yield |= exec_options_names[i].value;
				break;
			}
		if( !exec_options_names[i].name )
			log("unknown exec option '%s'", p);
	}

	return yield;
}

void exec_options_init(s_exec_options *eopt)
{
	memset(eopt, '\0', sizeof(s_exec_options));
	eopt->umask    = EXEC_DEFAULT_UMASK;
	eopt->envp[0]  = NULL;
}

void exec_options_deinit(s_exec_options *eopt)
{
	int i;
	
	if( eopt->command )
		free(eopt->command);
	
	for( i = 0; eopt->envp[i]; i++ )
		free(eopt->envp[i]);
	
	memset(eopt, '\0', sizeof(s_exec_options));
}

void exec_env_add(s_exec_options *eopt, const char *name, const char *value)
{
	int i;

	for( i = 0; eopt->envp[i]; i++ ); /* Empty Loop */
	
	if( i < EXEC_MAX_NUM_ENVS )
	{
		eopt->envp[i] = xmalloc(strlen(name) + strlen(value) + 2);
		sprintf(eopt->envp[i], "%s=%s", name, value);
		eopt->envp[i+1] = NULL;
	}
}

void exec_options_set_command(s_exec_options *eopt, const char *command)
{
	char *copy, *p;
	
	if( *command == '[' )
	{
		copy = xstrcpy(command);
		p = strchr(copy, ']');
		if( !p )
			eopt->command = copy;
		else
		{
			*p++ = '\0';
			eopt->command = xstrcpy(string_trimleft(p));
			eopt->options = exec_options_parse(copy+1);
			free(copy);
		}
	}
	else
		eopt->command = xstrcpy(command);
}

int exec_redirect_descriptor(int desc, const char *fname, int flags)
{
	int fd;
	
	(void)close(desc);

	if( (fd = open(fname, flags, S_IRUSR|S_IWUSR)) == -1 )
	{
		logerr("exec error: cannot open \"%s\"", fname);
		return -1;
	}

	if( fd != desc )
	{
		log("exec error: cannot open \"%s\" <--> %d (got %d)",
				fname, desc, fd);
		return -1;
	}

	return 0;
}

int exec_command(s_exec_options *eopt)
{
	pid_t pid = 0;
	int status = 0;
	time_t starttime = 0;
	time_t timer = 0;
	char *tempfile = NULL;
	struct sigaction new_chld;
	struct sigaction old_chld;
	sigset_t new_mask;
	sigset_t old_mask;
	
	if( eopt->command == NULL )
		return -1;
	
	if( eopt->options & EXEC_OPT_LOGOUT )
	{
		tempfile = file_gettmp();
		if( !tempfile )
			log("cannot generate temporary file name");
	}

	/* Block SIGCHLD */
	sigemptyset(&new_mask);
	sigaddset(&new_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &new_mask, NULL);
	
	/* Set the default SIGCHLD handler */
	new_chld.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &new_chld, &old_chld);

	if( (pid = fork()) == -1 )
	{
		logerr("exec error: cannot fork()");
		return -1;
	}

	if( !pid )
	{
		if( exec_redirect_descriptor(0, "/dev/null", O_RDONLY) == -1 )
			exit(128);
		
		if( tempfile )
		{
			if( exec_redirect_descriptor(1, tempfile, O_WRONLY|O_CREAT|O_TRUNC) == -1
			 || exec_redirect_descriptor(2, tempfile, O_WRONLY) == -1 )
				exit(128);
		}
		else
		{
			if( exec_redirect_descriptor(1, "/dev/null", O_WRONLY) == -1
			 || exec_redirect_descriptor(2, "/dev/null", O_WRONLY) == -1 )
				exit(128);
		}

		if( eopt->options & EXEC_OPT_SETSID )
			setsid();
		
		if( eopt->options & EXEC_OPT_USESHELL )
			execle("/bin/sh", "sh", "-c", eopt->command, NULL, eopt->envp);
		else
		{
			char *argv[EXEC_MAX_NUM_ARGS+1];
			int argc = string_parse_regular(argv, EXEC_MAX_NUM_ARGS,
			                                eopt->command);
			if( argc > 0 )
			{
				argv[argc] = NULL;
				execve(argv[0], argv, eopt->envp);
			}
			/* eopt->command is corrupted now */
		}
		
		/* We get here only in case of errors */
		logerr("cannot execute \"%s\"", eopt->command);
		
		exit(128);
	}
	
	/*
	 * We are inside parent process, do:
	 * 
	 * 1) if OUTMODE_LOGPIPE is used than log child process ouput
	 * 2) wait for child process to exit
	 */

	log("running \"%s\", PID %d",
		string_printable(eopt->command), (int)pid);

	if( !(eopt->options & EXEC_OPT_NOWAIT) )
	{
		starttime = time(NULL); /* Set process start time */
		
		if( waitpid(pid, &status, 0) > 0 )
		{
			eopt->runtime = time(NULL) - starttime;
	
			if( eopt->runtime < 0 )
				eopt->runtime = 0;

			/*
			 * Write process's output to the log file
			 */
			if( tempfile )
			{
				char buf[256];
				FILE *fp = file_open(tempfile, "r");

				if( !fp )
					logerr("cannot open temporary file \"%s\"", tempfile);
				else
				{
					while( fgets(buf, sizeof(buf), fp) )
					{
						string_chomp(buf);
						log("[%d] %s", pid, string_printable(buf));
					}
					file_close(fp);
				}
			}

			if( WIFEXITED(status) )
			{
				eopt->retc = WEXITSTATUS(status);
				log("process %d exit with code %d (%d seconds)",
					(int)pid, eopt->retc, eopt->runtime);
			}
			else if( WIFSIGNALED(status) )
			{
				eopt->retc = -1;
				log("process %d terminated on signal %d (%d seconds)",
					(int)pid, WTERMSIG(status), eopt->runtime);
			}
			else
			{
				eopt->retc = -1;
				log("process %d return with unknown status (%d seconds)",
					(int)pid, eopt->runtime);
			}
		}
		else
		{
			eopt->retc = -1;
			logerr("waitpid return error for PID %d", (int)pid);
		}
	}

	if( tempfile )
	{
		if( unlink(tempfile) == -1 && errno != ENOENT )
			logerr("cannot unlink temporary file \"%s\"", tempfile);
		free(tempfile);
	}

	/* Restore the original SIGCHLD handler */
	sigaction(SIGCHLD, &old_chld, NULL);

	/* Unblock SIGCHLD */
	sigprocmask(SIG_UNBLOCK, &new_mask, NULL);
	
	return eopt->retc;
}

/*
 *  Return -1 if error occured while excuting command, other values
 *  are return codes of your command
 */
int xsystem(const char *command, const char *p_input, const char *p_output)
{
	pid_t pid;
	int status;

	ASSERT(command != NULL);
	
	DEB((D_INFO, "xsystem: command \"%s\", input \"%s\", output \"%s\"",
		command, p_input, p_output));

	switch(pid=fork()) {
	case -1:
		return(-1);
	case  0:	
		if( p_input )
		{
			close(0);
			if( open(p_input, O_RDONLY) != 0 )
			{
				logerr("can't open stdin \"%s\"", p_input);
				exit(-1);
			}
		}
		if( p_output )
		{
			close(1);
			if( open(p_output, O_WRONLY|O_APPEND|O_CREAT, 0600) != 1 )
			{
				logerr("can't open stdout \"%s\"", p_output);
				exit(-1);
			}
		}
		if( p_output )
		{
			close(2);
			if( open(p_output, O_WRONLY|O_APPEND|O_CREAT, 0600) != 2 )
			{
				logerr("can't open stderr \"%s\"", p_output);
				exit(-1);
			}
		}
#ifdef SHELL
		execl(SHELL, "sh", "-c", command, NULL);
#else
		execl("/bin/sh", "sh", "-c", command, NULL);
#endif
		exit( (errno == 0)?0:-1 );	
	}

	if( waitpid(pid, &status, 0) == pid && WIFEXITED(status) )
	{
		return(WEXITSTATUS(status));
	}
	return(-1);
}

#ifndef HAVE_RENAME
int rename(const char *old_name, const char *new_name)
{
	int rc;

	ASSERT(old_name != NULL && new_name != NULL);
	
	if( (rc = link(old_name, new_name)) == 0 )
	{
		if( (rc = unlink(old_name)) == -1 )
			{ logerr("can't unlink file \"%s\"", old_name); }
		return 0;
	}
	else
	{
		return -1;
	}
}
#endif

/* ------------------------------------------------------------------------- */
/* Get free space on device mounted at path $path                            */
/* ------------------------------------------------------------------------- */
#if defined(HAVE_STATFS) || defined(HAVE_STATVFS) 
size_t getfreespace(const char *path)
{
#ifdef HAVE_STATVFS
	struct statvfs sfs;
#else
	struct statfs sfs;
#endif

	ASSERT(path != NULL);
	
#ifdef HAVE_STATVFS
	if( statvfs(path, &sfs) == 0 )
#else
	if( statfs(path, &sfs) == 0 )
#endif
	{
		return(sfs.f_bsize * sfs.f_bavail);
	}
	else
	{
		logerr("can't statfs \"%s\", assume enough space", path);
		return(~0L);
	}
}
#else
size_t getfreespace(const char *path)
{
	ASSERT(path != NULL);
	
	log("warning: fake getfreespace - assume enough space");
	return(~0L);
}
#endif

#ifndef HAVE_SETPROCTITLE

/*
 * clobber argv so ps will show what we're doing.
 * (stolen from BSD ftpd where it was stolen from sendmail)
 * warning, since this is usually started from inetd.conf, it
 * often doesn't have much of an environment or arglist to overwrite.
 */

static char *cmdstr    = NULL;
static char *cmdstrend = NULL;

void setargspace(char *argv[], char *envp[])
{
	cmdstr = argv[0];
	while( *envp ) envp++;
	envp--;
	cmdstrend = (*envp) + strlen(*envp);
}

void setproctitle(const char *fmt, ...)
{
	va_list args;
	char buf[256]; /* I hope it will be large enough */
	char *d = cmdstr;
	char *s = buf;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	
	/* Make ps print our process name */
	while( d < cmdstrend && *s ) { *d = *s; d++; s++; }
	while( d < cmdstrend ) *d++ = ' ';
}

#endif /* HAVE_SETPROCTITLE */
