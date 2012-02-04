/*
 *      binkleyforce -- unix FTN mailer project
 *
 *      Copyright (c) 1998-2000 Alexander Belkin, 2:5020/1398.11
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      $Id$
 */

#ifndef _NETSPOOL_H_
#define _NETSPOOL_H_

#define NS_NOTINIT	(0)
#define	NS_UNCONF	(1)
#define	NS_ERROR	(2)
#define	NS_READY	(3)
#define NS_RECEIVING	(4)
#define NS_RECVFILE	(5)

#define STRBUF (1024)

typedef struct {
	int	state;
	const char	*error;
	int	socket;
	char	filename[STRBUF];
	unsigned long long length;
} s_netspool_state;

/* create socket and start session */
void netspool_start(s_netspool_state *state, const char *host, const char *port, const char *address, const char *password);
/* request for outbound */
void netspool_query(s_netspool_state *state, const char *what);
/* receive next file */
void netspool_receive(s_netspool_state *state);
/* read data */
int netspool_read(s_netspool_state *state, void *buf, int buflen);
/* acknowledge successful file transmission */
void netspool_acknowledge(s_netspool_state *state);
/* end session */
void netspool_end(s_netspool_state *state);

#endif
