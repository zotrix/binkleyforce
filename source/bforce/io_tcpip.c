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

#define DEFAULT_PORT 60179	/* Birthday .. mother fucker :) */

static RETSIGTYPE tcpip_interrupt(int sig)
{
	tty_abort = TRUE;
	log("terminating on signal %d", sig);
}

static RETSIGTYPE tcpip_brokenpipe(int sig)
{
	tty_abort = TRUE;
	if( tty_online )
		log("connection closed");
	else
		log("terminating on signal %d", sig);
}

static int tcpip_connect2(struct sockaddr_in server)
{
	int fd;

	DEB((D_INFO, "tcpip_connect2: trying \"%s\" at port %d",
		inet_ntoa(server.sin_addr), (int)ntohs(server.sin_port)));

	if( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		logerr("can't create socket");
		return(1);
	}
	
	/* make new fd == stdin if it isn't already */
	if( fd > 0 )
	{
		(void)close(0);
		if( dup(fd) != 0 )
		{
			logerr("cannot dup socket to stdin");
			return(1);
		}
	}
	
	/* make stdout and stderr, too */
	(void)close(1);
	(void)close(2);
	
	if( dup(0) != 1 )
	{
		logerr("cannot dup stdin to stdout");
		return(1);
	}
	if( dup(0) != 2 )
	{
		logerr("cannot dup stdin to stderr");
		return(1);
	}
	
	if( fd > 2 ) (void)close(fd);

	/* switch off stdio buffering */
	setbuf(stdin,  (char *)NULL);
	setbuf(stdout, (char *)NULL);
	setbuf(stderr, (char *)NULL);
	
	clearerr(stdin);
	clearerr(stdout);
	clearerr(stderr);
	
	if( connect(0, (struct sockaddr*)&server, sizeof(server)) == -1 )
	{
		logerr("can't connect to %s", inet_ntoa(server.sin_addr));
		close(0);
		close(1);
		close(2);
		return 1;
	}

	if( tcpip_init() )
	{
		tcpip_shutdown();
		return 1;
	}

	log("TCP/IP connect to %s on port %d",
		inet_ntoa(server.sin_addr), (int)ntohs(server.sin_port));
	
	return(0);
}

int tcpip_connect(const char *hostname, e_tcpmode tcpmode)
{
	int rc = 0;
	struct hostent *he = NULL;
	struct servent *se = NULL;
	struct sockaddr_in server;
	char *host = xstrcpy(hostname);
	char *p = strrchr(host, ':');
	const char *port = NULL;

	server.sin_family = AF_INET;
	
	if( p )
		{ *p++ = '\0'; port = p; }
	else if( tcpmode == TCPMODE_BINKP )
		port = "binkp";
	else if( tcpmode == TCPMODE_TELNET )
		port = "telnet";
	else /* Default service name is "fido" */
		port = "fido";

	if( ISDEC(port) )
		server.sin_port = htons(atoi(port));
	else if( (se = getservbyname(port, "tcp")) )
		server.sin_port = se->s_port;
	else
		{ log("invalid port or service name \"%s\"", port); rc = 1; }
	
	if( rc == 0 )
	{
		if( (he = gethostbyname(host)) )
		{
			memcpy(&server.sin_addr, he->h_addr, he->h_length);
		}
		else
		{
			rc = 1;
			switch(h_errno) {
			case HOST_NOT_FOUND:
				log("host \"%s\" not found", host);
				break;
			case NO_ADDRESS:
				log("no IP address found for host \"%s\"", host);
				break;
			case NO_RECOVERY:
				log("non-recoverable name server error occured");
				break;
			case TRY_AGAIN:
				log("temporary error occured on name server");
				break;
			default:
				log("unknown error while resolving host \"%s\"", host);
				break;
			}
		}
	}

	if( host ) { free(host); host = NULL; }
	
	return rc ? rc : tcpip_connect2(server);
}

int tcpip_init(void)
{
	int nbio_arg = 1;
	int alive_arg = 1;
	struct linger lingeropt;

	tty_online = TRUE;
	tty_abort  = FALSE;
	tty_hangup = FALSE;
	tty_modem  = FALSE;
	
	/*
	 * Set sockets I/O to the non-blocking mode
	 */
	if( ioctl(0, FIONBIO, (char *)&nbio_arg, sizeof(nbio_arg)) == -1 )
	{
		logerr("failed to set non-blocking mode for stdin");
		return 1;
	}
	
	if( ioctl(1, FIONBIO, (char *)&nbio_arg, sizeof(nbio_arg)) == -1 )
	{
		logerr("failed to set non-blocking mode for stdout");
		return 1;
	}
	
	/*
	 * Set SO_LONGER socket option, so then we will close
	 * socket the system will block our close() call and
	 * try to deliver queued data.
	 */
	memset(&lingeropt, '\0', sizeof(struct linger));

	lingeropt.l_onoff  = 1;
	lingeropt.l_linger = 5*100; /* 5 seconds */
	
	if( setsockopt(1, SOL_SOCKET, SO_LINGER, (char*)&lingeropt, sizeof(struct linger)) == -1 )
	{
		logerr("failed to set SO_LINGER socket option for the stdout");
		return 1;
	}
	
	/*
	 * Set SO_KEEPALIVE socket option. The connection will be
	 * considered broken if remote side fail to respond on
	 * periodicaly transmitted message. We should not hang
	 * even without such features.
	 */
	if( setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char*)&alive_arg, sizeof(alive_arg)) == -1 )
	{
		logerr("failed to set SO_KEEPALIVE socket option");
		return 1;
	}
	
	signal(SIGHUP,  tcpip_interrupt);
	signal(SIGINT,  tcpip_interrupt);
	signal(SIGTERM, tcpip_interrupt);
	signal(SIGPIPE, tcpip_brokenpipe);
	
	return(0);
}

int tcpip_shutdown(void)
{
	close(0);
	close(1);
	close(2);
	return 0;
}

bool tcpip_isgood_host(const char *str)
{
	if( !str || !str[0] )
		return FALSE;
	
	if( str[0] == '-' && str[1] == '\0' )
		return FALSE;

	return TRUE;
}

