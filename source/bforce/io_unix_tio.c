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
 * Hardware handshake flags (stolen from mgetty)
 */
#ifdef CRTSCTS
# define TIO_FLOW_HARD     CRTSCTS /* linux, SunOS */
#else
# ifdef CRTSFL
#  define TIO_FLOW_HARD    CRTSFL /* SCO 3.2v4.2 */
# else
#  ifdef RTSFLOW
#   define TIO_FLOW_HARD   RTSFLOW | CTSFLOW /* SCO 3.2v2 */
#  else
#   ifdef CTSCD
#    define TIO_FLOW_HARD  CTSCD /* AT&T 3b1? */
#   else
#    define TIO_FLOW_HARD  0
#   endif
#  endif
# endif
#endif

/*
 * Software hadshake flags
 */
#define TIO_FLOW_SOFT      (IXON | IXOFF)

/*
 * Supported baud rates
 */
static struct speedtab {
#ifdef HAVE_TERMIOS_H
	speed_t	bspeed;
#else
	unsigned short bspeed;
#endif
	int nspeed;
const	char *speed;
} speedtab[] = {
	{ B300,    300,    "300"    },
#ifdef  B600
	{ B600,    600,    "600"    },
#endif
#ifdef	B900
	{ B900,    900,    "900"    },
#endif
	{ B1200,   1200,   "1200"   },
#ifdef  B1800
	{ B1800,   1800,   "1800"   },
#endif
	{ B2400,   2400,   "2400"   },
#ifdef	B3600
	{ B3600,   3600,   "3600"   },
#endif
	{ B4800,   4800,   "4800"   },
#ifdef	B7200
	{ B7200,   7200,   "7200"   },
#endif
	{ B9600,   9600,   "9600"   },
#ifdef	B14400
	{ B14400,  14400,  "14400"  },
#endif
#ifdef	B19200
	{ B19200,  19200,  "19200"  },
#endif	/* B19200 */
#ifdef	B28800
	{ B28800,  28800,  "28800"  },
#endif
#ifdef	B38400
	{ B38400,  38400,  "38400"  },
#endif	/* B38400 */
#ifdef	EXTA
	{ EXTA,    19200,  "EXTA"   },
#endif
#ifdef	EXTB
	{ EXTB,    38400,  "EXTB"   },
#endif
#ifdef	B57600
	{ B57600,  57600,  "57600"  },
#endif
#ifdef	B76800
	{ B76800,  76800,  "76800"  },
#endif
#ifdef	B115200
	{ B115200, 115200, "115200" },
#endif
#ifdef B230400
	{ B230400, 230400, "230400" },
#endif
#ifdef B460800
	{ B460800, 460800, "460800" },
#endif
	{ 0,       0,      ""       }
};


/* get current tio settings for given filedescriptor */

int tio_get(int fd, TIO *tio)
{
#ifdef HAVE_TERMIOS_H
	return tcgetattr(fd, tio);
#endif
}

/* set current tio settings for given filedescriptor */

int tio_set(int fd, TIO *tio)
{
#ifdef HAVE_TERMIOS_H
	return tcsetattr(fd, TCSANOW, tio);
#endif
}

int tio_set_speed(TIO *tio, unsigned int speed)
{
	int i;
#ifdef HAVE_TERMIOS_H
	speed_t bspeed = -1;
#else
	int bspeed = -1;
#endif
	
	for( i = 0; speedtab[i].nspeed; i++ )
	{
		if( speedtab[i].nspeed == speed )
		{
			bspeed = speedtab[i].bspeed;
			break;
		}
	}

	if( bspeed == -1 )
	{
		log("unsupported speed %d", speed);
		return -1;
	}

#ifdef HAVE_TERMIOS_H
	cfsetospeed(tio, bspeed);
	cfsetispeed(tio, bspeed);
#endif
	
	return 0;
}

/* get port speed. Return integer value, not symbolic constant */

int tio_get_speed(TIO *tio)
{
	int i;
#ifdef HAVE_TERMIOS_H
	speed_t bspeed = cfgetospeed(tio);
#endif
	
	for( i = 0; speedtab[i].nspeed; i++ )
	{
		if( speedtab[i].bspeed == bspeed )
			return speedtab[i].nspeed;
	}
	
	return-1;
}

int tio_set_flow_control(int fd, TIO *tio, int flow)
{
#if defined(HAVE_SYS_TERMIOX_H)
	struct termiox tix;
#endif

	DEB((D_MODEM, "tio_set_flow_control: setting HARD=%s, SOFT=%s",
			(flow & FLOW_HARD) ? "On" : "Off",
			(flow & FLOW_SOFT) ? "On" : "Off"));

#if defined(HAVE_TERMIOS_H)
	tio->c_cflag &= ~TIO_FLOW_HARD;
	tio->c_iflag &= ~TIO_FLOW_SOFT;
#ifdef IXANY
	tio->c_iflag &= ~IXANY;
#endif

	if( flow & FLOW_HARD )
		tio->c_cflag |= TIO_FLOW_HARD;
	if( flow & FLOW_SOFT )
		tio->c_iflag |= TIO_FLOW_SOFT;
#endif /* HAVE_TERMIOS_H */

#if defined(HAVE_SYS_TERMIOX_H)
	if( ioctl(fd, TCGETX, &tix) == -1 )
	{
		logerr("failed ioctl(TCGETX)");
		return -1;
	}
	
	if( flow & FLOW_HARD )
		tix.x_hflag |= (RTSXOFF | CTSXON);
	else
		tix.x_hflag &= ~(RTSXOFF | CTSXON);
			
	if( ioctl(fd, TCSETX, &tix) == -1 )
	{
		logerr("failed ioctl(TCSETX)");
		return -1;
	}
#endif /* HAVE_SYS_TERMIOX_H */
	
	return 0;
}

void tio_set_raw_mode(TIO *tio)
{
#if defined(HAVE_TERMIOS_H)
	tio->c_cflag &= ~(CSTOPB | PARENB | PARODD);
	tio->c_cflag |=  (CS8 | CREAD | HUPCL);
	tio->c_iflag = TIO_FLOW_SOFT;
	tio->c_oflag = 0;
	tio->c_lflag = 0;
	tio->c_cc[VMIN] = 128;
	tio->c_cc[VTIME] = 1;
#endif
}

void tio_set_local(TIO *tio, bool local)
{
#if defined(HAVE_TERMIOS_H)
	if( local )
		tio->c_cflag |= CLOCAL;
	else
		tio->c_cflag &= ~CLOCAL;
#endif
}

int tio_set_dtr(int fd, bool on)
{
	int mctl = TIO_RS232_DTR;
	
	DEB((D_MODEM, "tio_setdtr: setting DTR = %s", on ? "On" : "Off"));
	
	if( on )
	{
		if( ioctl(fd, TIOCMBIS, &mctl ) < 0 )
		{
			logerr("cannot set DTR = On: TIOCMBIS failed");
			return -1;
		}
	}
	else
	{
		if( ioctl(fd, TIOCMBIC, &mctl ) < 0 )
		{
			logerr("cannot set DTR = Off: TIOCMBIC failed");
			return -1;
		}
	}
	
	return 0;
}

int tio_ctty(int fd)
{
	if( setsid() < 0 )
		logerr("cannot make myself session leader (setsid)");

#ifdef TIOCSCTTY
	if( ioctl(fd, TIOCSCTTY, 0) < 0 )
	{
		logerr("cannot set controlling tty (ioctl)");
		return -1;
	}
#endif
	
	return 0;
}

int tio_send_break(void)
{
#ifdef HAVE_TERMIOS_H
	if( tcsendbreak(1, 0) < 0 )
	{
		logerr("tcsendbreak(1,0) failed");
#else
	if( ioctl(1, TCSBRK) < 0 )
	{
		logerr("ioctl(1,TCSBRK) failed");
#endif
		return -1;
	}
	return 0;
}

int tio_get_dcd(int fd)
{
	int flags;
	
	if( ioctl(fd, TIOCMGET, &flags ) < 0 )
		return -1;
	
	return (flags & TIO_RS232_DCD) ? 1 : 0;
}

int tio_get_rs232_state(void)
{
	int flags;
	
	if( ioctl(0, TIOCMGET, &flags ) < 0 )
		return -1;

	DEB((D_MODEM, "tio_getrs232: [%s]-[%s]-[%s]-[%s]-[%s]-[%s]",
		( flags & TIO_RS232_RTS ) ? "RTS" : "rts",
		( flags & TIO_RS232_CTS ) ? "CTS" : "cts",
		( flags & TIO_RS232_DSR ) ? "DSR" : "dsr",
		( flags & TIO_RS232_DTR ) ? "DTR" : "dtr",
		( flags & TIO_RS232_DCD ) ? "DCD" : "dcd",
		( flags & TIO_RS232_RNG ) ? "RNG" : "rng"));

	return flags;
}

