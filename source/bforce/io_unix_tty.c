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

#ifndef RX_BUFSIZE
#define RX_BUFSIZE	4096
#endif
#ifndef TX_BUFSIZE
#define TX_BUFSIZE	4096
#endif

int tty_status = TTY_SUCCESS;
int tty_hangup = 0;
int tty_abort  = 0;
int tty_online = 0;
int tty_modem  = 0;

static unsigned char rx_buf[RX_BUFSIZE];
static unsigned char tx_buf[TX_BUFSIZE];
static unsigned int rx_pos = 0;
static unsigned int tx_pos = 0;
static unsigned int rx_left = 0;		/* Received bytes left */
static unsigned int tx_left = TX_BUFSIZE;	/* Free space left */

#ifdef MODEM_WATCH_CARRIER
# define CARRIER_CHECK() if( tty_modem && tty_online && !tty_hangup \
                          && !tio_get_dcd(0) ) { \
                             log("carrier lost"); tty_hangup = 1; }
#else
# define CARRIER_CHECK()
#endif

const char *tty_errstr(int status)
{
	const char *msg;
	
	switch(status) {
	case TTY_SUCCESS:	msg = "No errors"; break;
	case TTY_TIMEOUT:	msg = "Time Out"; break;
	case TTY_HANGUP:	msg = "Hanged Up"; break;
	case TTY_ERROR:		msg = "IO Error"; break;
	default:		msg = "Unknown error"; break;
	}

	return msg;
}

/*
 * On success, tty_select() return zero value and set/reset
 * rd and wr to the appropriate values
 */
int tty_select(bool *rd, bool *wr, int timeout)
{
	fd_set rfds, wfds;
	struct timeval tv;
	int rc;
	
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	if( rd ) { FD_SET(0,&rfds); *rd = FALSE; }
	if( wr ) { FD_SET(1,&wfds); *wr = FALSE; }
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	CARRIER_CHECK();
	
	if( (tty_online && tty_hangup) || tty_abort )
		{ return( tty_status = TTY_HANGUP ); }
	
	rc = select(2, &rfds, &wfds, NULL, &tv);
	
	CARRIER_CHECK();

	if( rc < 0 )
	{
		if( errno == EINTR )
			if( (tty_online && tty_hangup) || tty_abort )
				{ tty_status = TTY_HANGUP; }
			else
				{ tty_status = TTY_SUCCESS; }
		else
			{ tty_status = TTY_ERROR; }
	}
	else if( rc == 0 )
	{
		tty_status = TTY_TIMEOUT;
	}
	else /* ( rc > 0 ) */
	{
		tty_status = TTY_ERROR;
		if( rd && FD_ISSET(0,&rfds) )
			{ tty_status = TTY_SUCCESS; *rd = TRUE; }
		if( wr && FD_ISSET(1,&wfds) )
			{ tty_status = TTY_SUCCESS; *wr = TRUE; }
	}
	
	DEB((D_TTYIO, "tty_select: return code = %s, rc = %d (rd=%s, wr=%s)",
		tty_errstr(tty_status), rc,
		rd ? *rd ? "true" : "false" : "null",
		wr ? *wr ? "true" : "false" : "null"));
	
	return tty_status;
}

/*
 * It is a frontend to tty_select(), that also checks our program buffer
 */
int tty_xselect(bool *rd, bool *wr, int timeout)
{
	if( rd && rx_pos > 0 )
	{
		(void)tty_select(NULL, wr, 0); *rd = TRUE;
		return TTY_SUCCESS;
	}
	return tty_select(rd, wr, timeout);
}

/*
 * Return non-zero value if some data available for reading
 * in input queue OR in our RX buffer
 */
int tty_charwait(int timeout)
{
	bool rd = FALSE;
	
	if( rx_pos > 0 )
		return 1;
	else if( tty_select(&rd, NULL, timeout) == 0 && rd == TRUE )
		return 1;
	else
		return 0;
}

/*
 * On success return number of bytes received
 */
int tty_read(unsigned char *buf, size_t size)
{
	int rc;
	
	DEB((D_TTYIO, "tty_read: want read %d byte(s)", size));
	
	CARRIER_CHECK();

	if( (tty_online && tty_hangup) || tty_abort )
		{ return( tty_status = TTY_HANGUP ); }

	rc = read(0, buf, size);
			
	CARRIER_CHECK();

	if( (tty_online && tty_hangup) || tty_abort )
	{
		tty_status = TTY_HANGUP;
	}
	else if( rc < 0 )
	{
		if( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
			{ tty_status = TTY_TIMEOUT; }
		else
			{ tty_status = TTY_ERROR; }
	}
	else /* ( rc >= 0 ) */
		tty_status = TTY_SUCCESS;

	DEB((D_TTYIO, "tty_read: return code = %s, rc = %d",
		tty_errstr(tty_status), rc));
	
	return tty_status == TTY_SUCCESS ? rc : tty_status;
}

/*
 * On success return number of bytes received
 */
int tty_read_timeout(unsigned char *buf, size_t size, int timeout)
{
	int rc;
	bool rd = FALSE;
	
	tty_status = TTY_SUCCESS;

	DEB((D_TTYIO, "tty_read_timeout: want read %d byte(s), timeout = %d", size, timeout));
	
	if( timeout > 0 )
	{
		if( (rc = tty_select(&rd, NULL, timeout)) < 0 )
			return rc;
		
		if( rd )
		{
			if( (rc = tty_read(buf, size)) == 0 )
				return TTY_ERROR;
			
			return rc;
		}
		
		return TTY_TIMEOUT;
	}

	return tty_read(buf, size);
}

int tty_write(const unsigned char *buf, size_t size)
{
	int rc;

	DEB((D_TTYIO, "tty_write: want write %d byte(s)", size));
	
	CARRIER_CHECK();

	if( (tty_online && tty_hangup) || tty_abort )
		{ return(tty_status = TTY_HANGUP); }

	rc = write(1, buf, size);

	CARRIER_CHECK();

	if( rc < 0 )
	{
		if( errno == EINTR )
		{
			if( (tty_online && tty_hangup) || tty_abort )
				{ tty_status = TTY_HANGUP; }
			else
				{ tty_status = TTY_TIMEOUT; }
		}
		else if( errno == EAGAIN || errno == EWOULDBLOCK )
		{
			tty_status = TTY_TIMEOUT;
		}
		else if( errno == EPIPE )
		{
			tty_hangup = 1;
			tty_status = TTY_HANGUP;
		}
		else
		{
			tty_status = TTY_ERROR;
		}
	}
	else if( rc == 0 )
		tty_status = TTY_TIMEOUT;
	else /* ( rc > 0 ) */
		tty_status = TTY_SUCCESS;

	DEB((D_TTYIO, "tty_write: return code = %s, rc = %d",
		tty_errstr(tty_status), rc));
	
	return (tty_status == TTY_SUCCESS) ? rc : tty_status;
}

/*
 * On success return number of bytes sent
 */
int tty_write_timeout(const unsigned char *buf, size_t size, int timeout)
{
	time_t timer;
	int rc, pos = 0;
	
	DEB((D_TTYIO, "tty_write_timeout: want write %d byte(s), timeout = %d", size, timeout));
	
	tty_status = TTY_SUCCESS;
	timer_set(&timer, timeout);
	
	while( pos < size )
	{
		if( timer_expired(timer) )
		{
			break;
		}
		else if( (rc = tty_write(buf + pos, size - pos)) < 0 )
		{
			if( rc == TTY_TIMEOUT )
				usleep(10000);	/* 0.01 sec */
			else
				return rc;
		}
		else /* ( rc > 0 ) */
		{
			DEB((D_TTYIO, "tty_write_timeout: written %d byte(s)", rc));
			pos += rc;
		}
	}
	
	return (pos > 0) ? pos : TTY_TIMEOUT;
}

int tty_putc(unsigned char ch, int timeout)
{
	return tty_write_timeout(&ch, 1, timeout);
}

int tty_puts(const unsigned char *s, int timeout)
{
	return tty_write_timeout(s, strlen(s), timeout);
}

int tty_getc(int timeout)
{
	int rc;

	tty_status = TTY_SUCCESS;
	
	if( rx_left == 0 )
	{
		rx_pos = 0;
		if( (rc = tty_read_timeout(rx_buf, sizeof(rx_buf), timeout)) < 0 )
		{
			DEB((D_TTYIO, "tty_getc: tty_read() result \"%s\"", tty_errstr(rc)));
			return rc;
		}
		else if( rc == 0 )
			return TTY_ERROR; /* Isn't it? */
		
		rx_left = rc;
	}
		
	rx_left--;
	return rx_buf[rx_pos++];
}

int tty_bufc(unsigned char ch, int timeout)
{
	int rc;
	
	if( tx_left == 0 )
	{
		if( (rc = tty_flushbuf(timeout)) < 0 ) return rc;
		else if( rc == 0 ) return TTY_TIMEOUT;
	}
	
	tx_buf[tx_pos++] = ch;
	--tx_left;
	
	return(tty_status = TTY_SUCCESS);
}

int tty_flushout(void)
{
	DEB((D_TTYIO, "tty_flushout: flushing out"));

	CARRIER_CHECK();

	if( (tty_online && tty_hangup) || tty_abort )
		return(tty_status = TTY_HANGUP);

#ifdef FUCKING_TCDRAIN
	if( tty_modem )
	{
		while( tcdrain(1) == -1 )
		{
			if( errno == EINTR )
			{
				if( (tty_online && tty_hangup) || tty_abort )
					return(tty_status = TTY_HANGUP);
			}
			else if( errno == EPIPE )
				return(tty_status = TTY_HANGUP);
			else if( errno == EAGAIN || errno == EWOULDBLOCK )
				return(tty_status = TTY_SUCCESS);
			else
				return(tty_status = TTY_ERROR);
		}
	}
#endif /* FUCKING_TCDRAIN */
	
	return(tty_status = TTY_SUCCESS);
}

int tty_flushbuf(int timeout)
{
	int rc = 0;
	
	tty_status = TTY_SUCCESS;

	DEB((D_TTYIO, "tty_flushbuf: flushing internal TX buffer"));

	if( tx_pos )
	{
		if( (rc = tty_write_timeout(tx_buf, tx_pos, timeout)) < 0 )
		{
			return rc;
		}
		tx_pos  -= rc;
		tx_left += rc;
	}

	return rc;
}

int tty_clearin(void)
{
	DEB((D_TTYIO, "tty_clearin: clear RX buffers"));

	tcflush(0, TCIFLUSH);
	
	rx_left = 0;
	rx_pos = 0;

	CARRIER_CHECK();
	
	if( (tty_online && tty_hangup) || tty_abort )
		return(tty_status = TTY_HANGUP);
	
	return(tty_status = TTY_SUCCESS);
}

int tty_clearout(void)
{
	DEB((D_TTYIO, "tty_clearout: clear TX buffers"));

	tcflush(1, TCOFLUSH);

	tx_left = TX_BUFSIZE;
	tx_pos = 0;

	CARRIER_CHECK();

	if( (tty_online && tty_hangup) || tty_abort )
		return(tty_status = TTY_HANGUP);
	
	return(tty_status = TTY_SUCCESS);
}
