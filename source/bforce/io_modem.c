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

const char *modem_errlist[] = 
{
	"No error",
	"Modem return ERROR",
	"Can't send string to modem",
	"Modem not response",
	NULL
};

/* ------------------------------------------------------------------------- */
/*  Get connect speed from connect string returned by modem                  */
/* ------------------------------------------------------------------------- */
long modem_getconnspeed(const char *connstr)
{
	const char *p;
	
	for( p = connstr; *p; p++ )
	{
		if( isdigit(*p) )
			return atol(p);
	}
	
	return 0L;
}

bool modem_isgood_phone(const char *str)
{
	if( !str || !str[0] )
		return FALSE;
	
	if( str[0] == '-' && str[1] == '\0' )
		return FALSE;
	
	if( string_casestr(str, "unpublished") )
		return FALSE;

	if( string_casestr(str, "unknown") )
		return FALSE;
	
	if( string_casestr(str, "none") )
		return FALSE;

	if( string_casestr(str, "00-00-000000") )
		return FALSE;

	return TRUE;
}

/* ------------------------------------------------------------------------- */
/* Translate phone number using given rules, returned value must be free'ed! */
/* ------------------------------------------------------------------------- */
char *modem_transphone(char *buffer, const char *phone, size_t buflen)
{
	const char *p;
	s_cval_entry *ptrl;

	ASSERT(phone != NULL);
		
	DEB((D_MODEM, "translate_phone: want translate \"%s\"", phone));
	
	for( ptrl = conf_first(cf_phone_translate); ptrl;
	     ptrl = conf_next(ptrl) )
	{
		if( !ptrl->d.translate.find )
			continue;
		
		if( (p = strstr(phone, ptrl->d.translate.find)) )
		{
			DEB((D_MODEM, "translate_phone: replace \"%s\" with \"%s\"",
				ptrl->d.translate.find, ptrl->d.translate.repl));

			strnxcpy(buffer, phone, buflen);

			if( p - phone < buflen - 1 )
			{
				buffer[p - phone] = '\0';
				
				if( ptrl->d.translate.repl )
					strnxcat(buffer, ptrl->d.translate.repl, buflen);
				
				strnxcat(buffer, p + strlen(ptrl->d.translate.find), buflen);
			}
			
			DEB((D_MODEM, "translate_phone: result is \"%s\"", buffer));
			
			return buffer;
		}
	}

	return strnxcpy(buffer, phone, buflen);
}

/* ------------------------------------------------------------------------- */
/*  Send string to modem, using some control character sequences, like       */
/*  '\r' - CR, '\P' - phone number, etc..                                    */
/* ------------------------------------------------------------------------- */
int modem_putstr(const char *str)
{
	const char *ptr;
	int rc;
	bool flushed = FALSE;
	
	DEB((D_MODEM, "modem_putstr: want to send \"%s\"", str));

	rc  = TTY_SUCCESS;
	ptr = str;
	
	while( *ptr )
	{
		/* Flush buffer before "special" operations */
		if( !flushed && strchr("~`^v", *ptr) )
		{
			flushed = TRUE;
			if( (rc = tty_flushout()) < 0 )
				return rc;
		}
		
		switch( *ptr ) {
		case '~':
			sleep(1);
			break;
		case '`':
			usleep(250000L);
			break;
		case '^':
			rc = tio_set_dtr(0, 1);
			break;
		case 'v':
			rc = tio_set_dtr(0, 0);
			break;
		case '|':
		case '\r':
			rc = tty_putc('\r', 2);
			if( !rc && !(rc = tty_flushout()) )
			{
				/*
				 * Make sure that we will never send
				 * anything to the modem immediately
				 * after CR
				 */
			     /* i changed value for my modem
			      * (from  250000 */
				usleep(500000);
				flushed = TRUE;
			}
			break;
		case '\\':
			++ptr;
			switch(*ptr) {
			case '\0':
				rc = tty_putc('\\', 2);
				break;
			case 'r':
				rc = tty_putc('\r', 2);
				break;
			case 'n':
				rc = tty_putc('\n', 2);
				break;
			default:
				rc = tty_putc(*ptr, 2);
			}
			flushed = FALSE;
			break;
		default:
			rc = tty_putc(*ptr, 2);
			flushed = FALSE;
		}
		
		if( rc < 0 )
			return rc;
		
		++ptr;
	}
	
	if( !flushed && (rc = tty_flushout()) < 0 )
		return rc;
	
	return 0;
}

void modem_clearin(int timeout)
{
	time_t timer;

	timer_set(&timer, timeout);

	while( CHARWAIT(1) )
	{
		CLEARIN();
		
		if( timer_expired(timer) )
			return;
		
		usleep(100000); /* 0.1 seconds delay */
	}
}

/* ------------------------------------------------------------------------- */
/* Reads in at most one less than $bufsize characters from modem and         */
/* stores them into the buffer pointed to by $buf, check for timeout.        */
/* $timer must be set with tty_settimer() call!                              */
/* ------------------------------------------------------------------------- */
int modem_getline(char *buf, int bufsize, time_t timer)
{
	int rc = 0, pos = 0;
	
	ASSERT(buf != NULL || bufsize == 0);
	
	while(1)
	{
		if( timer_expired(timer) )
			return TTY_TIMEOUT;

		if( (rc = tty_getc(1)) < 0 && rc != TTY_TIMEOUT )
			return rc;
		else if( rc == '\r' || rc == '\n' )
			return pos;
		else if( rc > 0 )
		{
			if( pos < bufsize-1 )
			{
				buf[pos++] = rc;
				buf[pos  ] = '\0';
			} else
				return pos;
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Dial using phone number $phone, if connection will be established -       */
/* put connect string to *connstr (must be freed)                            */
/* ------------------------------------------------------------------------- */
int modem_dial(const char *dialstr, int timeout, char **connstr)
{
	s_cval_entry *ptrl;
	char buf[MODEM_MAX_RESP+1];
	time_t timer;
	int len = 0;

	ASSERT(dialstr != NULL && connstr != NULL);
	
	*connstr = NULL;
	
	if( modem_putstr(dialstr) != TTY_SUCCESS )
	{
		log("error sending dial string \"%s\"", dialstr);
		return -1;
	}
	
	timer_set(&timer, timeout);

	while(1)
	{
		if( timer_expired(timer) )
		{
			log("dialing timed out");
			return -1;
		}
		
		if( (len = modem_getline(buf, sizeof(buf), timer)) < 0 )
		{
			if( len == TTY_TIMEOUT )
				log("dialing timed out");
			
			return -1;
		}
		else if( len > 0 )
		{
			DEB((D_MODEM, "modem_dial: got \"%s\"", buf));
			
			for( ptrl = conf_first(cf_modem_dial_response); ptrl;
			     ptrl = conf_next(ptrl) )
			{
				if( ptrl->d.dialresp.mstr && strstr(buf, ptrl->d.dialresp.mstr) )
				{
					if( ptrl->d.dialresp.retv == RESPTYPE_CONNECT )
					{
						*connstr = xstrcpy(buf);
						return 0;
					}
					return ptrl->d.dialresp.retv;
				}
			}
			if( *buf )
				log("modem: \"%s\"", string_printable(buf));
		}
	}
}

int modem_command(const char *command, int timeout, bool logit)
{
	time_t timer;
	size_t count = 0;
	int len = 0;
	char buffer[MODEM_MAX_RESP+1];
	char cmdstr[MODEM_MAX_COMMAND+1];
	char *n;
	char *p;

	ASSERT(command != NULL);
	
	strnxcpy(cmdstr, command, sizeof(cmdstr));
	
	DEB((D_MODEM, "modem_command: command string \"%s\"", cmdstr));

	CLEARIN();
	
	for( p = string_token(cmdstr, &n, NULL, 1); p;
	     p = string_token(NULL, &n, NULL, 1) )
	{
		DEB((D_MODEM, "modem_command: send \"%s\"",
				string_printable(p)));
		
		if( modem_putstr(p) < 0 )
			return MODEM_CANTSEND;
		
		count = 0;
		timer_set(&timer, timeout);
		
		while(1)
		{
			if( timer_expired(timer) )
				return MODEM_NORESP;
		
			if( (len = modem_getline(buffer, sizeof(buffer), timer)) < 0 )
			{
				return MODEM_NORESP;
			}
			else if( len > 0 )
			{
				count += len;
				
				if( count > MODEM_MAX_RESP_SIZE )
				{
					log("modem response exceeds limit %ld bytes",
							(long)count);
					return MODEM_NORESP;
				}
				
				DEB((D_MODEM, "modem_command: got \"%s\"",
					string_printable(buffer)));
				
				if( !strcmp(buffer, "ERROR") )
					return MODEM_ERROR;
				else if( !strcmp(buffer, "OK") )
					break;
				else if( logit )
					log("modem: \"%s\"", string_printable(buffer));
			}
		}
	}
	
	return MODEM_OK;
}

int modem_hangup(const char *command, int timeout)
{
#ifdef MODEM_HANGUP_WATCH_CARRIER
	time_t timer;
	
	ASSERT(command != NULL);

	if( tio_get_dcd(0) == 1 )
	{
		DEB((D_MODEM, "modem_hangup: send \"%s\"", command));
		
		if( modem_putstr(command) < 0 )
			return MODEM_CANTSEND;
	
		timer_set(&timer, timeout);

		while( tio_get_dcd(0) == 1 )
		{
			if( timer_expired(timer) )
				return MODEM_NORESP;
			
			sleep(1);
		}
	}

	sleep(2); CLEARIN(); CLEAROUT();

	return MODEM_OK;
#else /* MODEM_HANGUP_WATCH_CARRIER */
	return modem_command(command, timeout, FALSE);
#endif
}

bool modem_candialout(const char *modemdev)
{
	char tmp[BF_MAXPATH+1];
	const char *p_nodial = conf_string(cf_nodial_flag);
	char *p = NULL;
	
	if( p_nodial && *p_nodial )
	{
		if( access(p_nodial, F_OK) == 0 )
			return FALSE;

		strnxcpy(tmp, p_nodial, sizeof(tmp));
		strnxcat(tmp, ".", sizeof(tmp));
		strnxcat(tmp, (p = port_get_name(modemdev)), sizeof(tmp));

		if( p )
			free(p);

		if( access(tmp, F_OK) == 0 )
			return FALSE;
	}
	
	return TRUE;
}

/* ------------------------------------------------------------------------- */
/* Return pointer to the first not locked now tty                            */
/* ------------------------------------------------------------------------- */
s_modemport *modem_getfree_port(const char *lockdir)
{
	s_cval_entry *ptrl;
		
	/* Find first not locked modem device */
	for( ptrl = conf_first(cf_modem_port); ptrl; ptrl = conf_next(ptrl) )
	{
		if( port_checklock(lockdir, &ptrl->d.modemport) == LOCKCHECK_NOLOCK
		 && modem_candialout(ptrl->d.modemport.name) == TRUE )
			return &ptrl->d.modemport;
	}
	
	return NULL;
}

/* ------------------------------------------------------------------------- */
/* Return pointer to the first tty whose name contain substring `ttyname'    */
/* ------------------------------------------------------------------------- */
s_modemport *modem_getmatch_port(const char *substr)
{
	s_cval_entry *ptrl;
	
	/* Find first matching modem device */
	for( ptrl = conf_first(cf_modem_port); ptrl; ptrl = conf_next(ptrl) )
	{
		if( strstr(ptrl->d.modemport.name, substr) )
			return &ptrl->d.modemport;
	}
	
	return NULL;
}

