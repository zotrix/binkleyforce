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
#include "io.h"

/*
 * Get verbal device name, remove "/dev/" part and replace all '/'
 * characters by the '-' character. Returned value must be freed.
 */
char *port_get_name(const char *devname)
{
	char *yield = NULL;
	
	if( !devname )
		yield = xstrcpy("unknown");
	else if( !strncmp(devname, "/dev/", 5) )
		yield = xstrcpy(devname + 5);
	else
		yield = xstrcpy(devname);

	string_replchar(yield, '/', '-');

	return yield;
}

#ifndef MODEM_WATCH_CARRIER
static RETSIGTYPE linedrop(int sig)
{
	if( tty_online )
	{
		log("carrier lost");
		tty_hangup = TRUE;
	}
	else
	{
		log("terminating on signal %d", sig);
		tty_abort = TRUE;
	}
}
#endif

static RETSIGTYPE interrupt(int sig)
{
	tty_abort = TRUE;
	log("terminating on signal %d", sig);
}

/*
 *  Open modem device $port, save old termios to $oldtio
 */
int port_open(const s_modemport *port, int detach, TIO *oldtio)
{
	int fd;
	
	ASSERT(port != NULL);
	
	DEB((D_MODEM, "openport: open device \"%s\" locked at %ld",
		port->name, (long)port->speed));
	
	if( (fd = open(port->name, O_RDWR | O_NONBLOCK)) < 0 )
		{ logerr("cannot open device \"%s\"", port->name); return 1; }

	/* make new fd == stdin if it isn't already */
	if( fd > 0 )
	{
		if( detach )
			(void)close(0);
		if( dup(fd) != 0 )
			{ logerr("cannot make device \"%s\" stdin", port->name); return(1); }
	}
	
	/* make stdout and stderr, too */
	if( detach )
		{ (void)close(1); (void)close(2); }
	
	if( dup(0) != 1 )
		{ logerr("cannot dup stdin to stdout"); return(1); }
	
	if( dup(0) != 2 )
		{ logerr("cannot dup stdin to stderr"); return(1); }
	
	if( fd > 2 )
		(void)close(fd);

	/* Set port modes (LOCAL mode) */
	if( port_init(0, port->speed, oldtio, TRUE) )
		return(1);
	
	/* Make it controlling tty */
	if( detach )
		tio_ctty(0);
	
	/*
	 * Switch off stdio buffering. We are
	 * not going to use streamed I/O, but..
	 */
	setbuf(stdin,  (char *)NULL);
	setbuf(stdout, (char *)NULL);
	setbuf(stderr, (char *)NULL);
	
	return(0);
}

/*
 *  Set terminal options as required. Don't forget that settings of
 *  the file descriptor copies made with dup() are shared.
 */
int port_init(int fd, int portspeed, TIO *oldtio, bool local)
{
	TIO tio;
	
	/* Install signal handlers */
#ifdef MODEM_WATCH_CARRIER
	signal(SIGHUP, interrupt);
#else
	signal(SIGHUP, linedrop);
#endif
	signal(SIGINT, interrupt);
	signal(SIGTERM, interrupt);
	
	/*
	 * Set descriptor I/O to the non-blocking mode
	 */
	if( fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1 )
	{
		logerr("cannot set non-blocking mode for stdin");
		return -1;
	}
	
	if( isatty(fd) )
	{
		/* Report to tty I/O routines that we are using modem */
		tty_modem = 1;
	
		if( tio_get(fd, &tio) == -1 )
		{
			logerr("initport error: cannot get terminal settings");
			return -1;
		}
		
		/*
		 * Save old settings. We will restore them on exit.
		 */
		if( oldtio )
			*oldtio = tio;
		
		/* Set "raw" mode */
		tio_set_raw_mode(&tio);

		/* Set hardware flow control */
		if( tio_set_flow_control(fd, &tio, FLOW_HARD) == -1 )
			return -1;
		
		/* Should we ignore DCD? */
#ifdef MODEM_WATCH_CARRIER
		tio_set_local(&tio, TRUE);
#else
		tio_set_local(&tio, local);
#endif
		
		/* Set port speed */
		if( portspeed )
		{
			/* set bit rate */
			tio_set_speed(&tio, portspeed);
		}
	
		if( tio_set(fd, &tio) == -1 )
		{
			logerr("initport error: cannot set terminal settings");
			return -1;
		}
	}

	return 0;
}

int port_carrier(int fd, bool online)
{
	tty_online = online;

#ifndef MODEM_WATCH_CARRIER
	if( isatty(fd) )
	{
		TIO tio;
		
		if( tio_get(fd, &tio) == -1 )
		{
			logerr("port_carrier error: cannot get terminal settings");
			return -1;
		}
		
		tio_set_local(&tio, online ? FALSE : TRUE);
			
		if( tio_set(fd, &tio) == -1 )
		{
			logerr("port_carrier error: cannot set terminal settings");
			return -1;
		}
	}
#endif

	return 0;
}

/*
 *  Restore old terminal settings
 */
int port_deinit(int fd, TIO *oldtio)
{
	if( tio_set(fd, oldtio) == -1 )
	{
		logerr("deinitport error: cannot restore terminal settings");
		return -1;
	}
	
	return 0;
}

/*
 *  Close port
 */
void port_close(void)
{
	DEB((D_MODEM, "closeport: close modem port"));
	/* Do nothing */
}

