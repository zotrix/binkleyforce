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

#ifndef _OS_H_
#define _OS_H_

#define EXEC_OPT_NOWAIT         0x01
#define EXEC_OPT_SETSID         0x02
#define EXEC_OPT_LOGOUT         0x04
#define EXEC_OPT_USESHELL       0x08

#define EXEC_MAX_NUM_ENVS       40
#define EXEC_MAX_NUM_ARGS       20
#define EXEC_DEFAULT_UMASK      ~(S_IRUSR|S_IWUSR);

struct exec_options
{
	/*
	 * This variables must be set before running
	 */
	char *command;
	char *envp[EXEC_MAX_NUM_ENVS+1];
	int   umask;
	int   timeout;
	int   options;
	
	/*
	 * This variables will store command execution status
	 */
	int   retc;
	int   runtime;
};
typedef struct exec_options s_exec_options;

int  exec_file_exist(const char *command);
void exec_options_init(s_exec_options *eopt);
void exec_options_deinit(s_exec_options *eopt);
void exec_env_add(s_exec_options *eopt, const char *name, const char *value);
void exec_options_set_command(s_exec_options *eopt, const char *command);
int  exec_command(s_exec_options *eopt);

#if defined(OS2) || defined(W32)
#define DIRSEPCHR		'\\'
#define DIRSEPSTR		"\\"
#else /* Unices */
#define DIRSEPCHR		'/'
#define DIRSEPSTR		"/"
#endif

size_t getfreespace(const char *path);
int xsystem(const char *command, const char *p_input, const char *p_output);

#ifndef HAVE_SETPROCTITLE
void setargspace(char *argv[], char *envp[]);
void setproctitle(const char *fmt, ...);
#endif

#ifndef HAVE_RENAME
int rename(const char *old_name, const char *new_name);
#endif

#endif /* _OS_H_ */
