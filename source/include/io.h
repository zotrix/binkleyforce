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

#ifndef _IO_H_
#define _IO_H_

typedef enum {
	TCPMODE_RAW,
	TCPMODE_TELNET,
	TCPMODE_BINKP
} e_tcpmode;

extern int tty_online;
extern int tty_hangup;
extern int tty_abort;
extern int tty_modem;
extern int tty_status;

#define XON			('Q' & 037)
#define XOFF			('S' & 037)
#define CPMEOF			('Z' & 037)
#define NUL			0x00
#define SOH			0x01
#define STX			0x02
#define ETX			0x03
#define EOT			0x04
#define ENQ			0x05
#define ACK			0x06
#define BEL			0x07
#define BS			0x08
#define HT			0x09
#define LF			0x0a
#define VT			0x0b
#define FF			0x0c
#define CR			0x0d
#define SO			0x0e
#define SI			0x0f
#define DLE			0x10
#define DC1			0x11
#define DC2			0x12
#define DC3			0x13
#define DC4			0x14
#define NAK			0x15
#define SYN			0x16
#define ETB			0x17
#define CAN			0x18
#define EM			0x19
#define SUB			0x1a
#define ESC			0x1b
#define FS			0x1c
#define GS			0x1d
#define RS			0x1e
#define US			0x1f
#define TSYNC			0xae
#define YOOHOO			0xf1

				/* DON'T CHANGE THIS VALUES! */
				/* SOME DON'T USE MACROS     */
#define TTY_SUCCESS		 0
#define TTY_TIMEOUT		-1
#define TTY_HANGUP		-2
#define TTY_ERROR		-3

#define XSELECT(a,b,c)		tty_xselect(a, b, c)
#define WRITE(a,b)		tty_write(a, b)
#define READ(a,b)		tty_read(a, b)
#define WRITE_TIMEOUT(a,b)	tty_write_timeout(a, b, 30)
#define READ_TIMEOUT(a,b,c)	tty_read_timeout(a, b, c)
#define GETCHAR(a)		tty_getc(a)
#define CHARWAIT(a)		tty_charwait(a)
#define PUTCHAR(a)		tty_putc(a, 30)
#define BUFCHAR(a)		tty_bufc(a, 30)
#define PUTSTR(a)		tty_puts(a, 30)
#define CLEARIN()		tty_clearin()
#define CLEAROUT()		tty_clearout()
#define FLUSHOUT()		tty_flushout()
#define FLUSHBUF()		tty_flushbuf(30)
#define DTRHIGH()		tio_setdtr(0, TRUE)
#define DTRLOW()		tio_setdtr(0, FALSE)
#define SENDBREAK()		tio_sendbreak()
#define TTYRESET() \
	{ \
		tty_abort = 0; \
		tty_status = TTY_SUCCESS; \
	}
#define TTYSTATUS(a) \
	{ \
		tty_online = a; \
		tty_hangup = 0; \
		tty_abort  = 0; \
		tty_status = TTY_SUCCESS; \
	}

/*
 * io_unix_tty.c
 */
int  tty_select(bool *rd, bool *wr, int timeout);
int  tty_xselect(bool *rd, bool *wr, int timeout);
int  tty_charwait(int timeout);
int  tty_read(unsigned char *buf, size_t size);
int  tty_read_timeout(unsigned char *buf, size_t size, int timeout);
int  tty_write(const unsigned char *buf, size_t size);
int  tty_write_timeout(const unsigned char *buf, size_t size, int timeout);
int  tty_putc(unsigned char ch, int timeout);
int  tty_puts(const unsigned char *s, int timeout);
int  tty_getc(int timeout);
int  tty_bufc(unsigned char ch, int timeout);
int  tty_flushout(void);
int  tty_flushbuf(int timeout);
int  tty_clearin(void);
int  tty_clearout(void);
const char *tty_errstr(int status);

/*
 * io_unix_lock.c
 */
#define LOCKCHECK_NOLOCK	0
#define LOCKCHECK_LOCKED	1
#define LOCKCHECK_ERROR		2
#define LOCKCHECK_OURLOCK	3

int  port_checklock(const char *lockdir, const s_modemport *modemport);
int  port_lock(const char *lockdir, const s_modemport *modemport);
int  port_unlock(const char *lockdir, const s_modemport *modemport);

/*
 * io_unix_tio.c
 */
#ifdef HAVE_TERMIOS_H
typedef struct termios TIO;
#endif

/*
 * Flow control flags
 */
#define FLOW_HARD  0x01
#define FLOW_SOFT  0x02

/*
 * Define our own RS232 line flags
 */
#ifdef TIOCM_DTR
# define TIO_RS232_DTR  TIOCM_DTR
# define TIO_RS232_DSR  TIOCM_DSR
# define TIO_RS232_RTS  TIOCM_RTS
# define TIO_RS232_CTS  TIOCM_CTS
# ifdef TIOCM_CAR
#  define TIO_RS232_DCD TIOCM_CAR
# else
#  define TIO_RS232_DCD TIOCM_CD
# endif
# ifdef TIOCM_RNG
#  define TIO_RS232_RNG TIOCM_RNG
# else
#  define TIO_RS232_RNG TIOCM_RI
# endif
#else /* TIOCM_DTR */
# define TIO_RS232_DTR  0x01
# define TIO_RS232_DSR  0x02
# define TIO_RS232_RTS  0x04
# define TIO_RS232_CTS  0x08
# define TIO_RS232_DCD  0x10
# define TIO_RS232_RNG  0x20
#endif /* TIOCM_DTR */

int  tio_get(int fd, TIO *tio);
int  tio_set(int fd, TIO *tio);
int  tio_set_speed(TIO *tio, unsigned int speed);
int  tio_get_speed(TIO *tio);
int  tio_set_flow_control(int fd, TIO *tio, int flow);
void tio_set_raw_mode(TIO *tio);
void tio_set_local(TIO *tio, bool local);
int  tio_set_dtr(int fd, bool on);
int  tio_ctty(int fd);
int  tio_send_break(void);
int  tio_get_dcd(int fd);
int  tio_get_rs232_state(void);

/*
 * io_tcpip.c
 */
int  tcpip_connect(const char *hostname, e_tcpmode tcpmode);
int  tcpip_init(void);
int  tcpip_shutdown(void);
bool tcpip_isgood_host(const char *str);

/*
 * io_unix_modem.c
 */
char *port_get_name(const char *devname);
int   port_open(const s_modemport *port, int detach, TIO *oldtio);
int   port_init(int fd, int portspeed, TIO *oldtio, bool local);
int   port_carrier(int fd, bool online);
int   port_deinit(int fd, TIO *oldtio);
void  port_close(void);

/*
 * io_modem.c
 */
#define MODEM_OK		0
#define MODEM_ERROR		1
#define MODEM_CANTSEND		2
#define MODEM_NORESP		3

extern const char *modem_errlist[];

long  modem_getconnspeed(const char *connstr);
bool  modem_isgood_phone(const char *str);
char *modem_transphone(char *buffer, const char *phone, size_t buflen);
void  modem_clearin(int timeout);
int   modem_putstr(const char *str);
int   modem_getline(char *buf, int bufsize, time_t timer);
int   modem_dial(const char *dialstr, int timeout, char **connstr);
int   modem_command(const char *command, int timeout, bool logit);
int   modem_hangup(const char *command, int timeout);
bool  modem_candialout(const char *modemdev);
s_modemport *modem_getfree_port(const char *lockdir);
s_modemport *modem_getmatch_port(const char *ttysubname);

#endif /* _IO_H_ */
