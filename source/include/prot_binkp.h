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
 *
 *	binkp's frames:
 *	
 *	  +---------------------- 0=data block, 1=message(command)
 *	  |                +---- data block size / msg's argument size
 *	  |                |
 *	  7  6543210 76543210
 *	 +-+-------+--------+--- ... ---+
 *	 | |   HI      LO   |           | -- data block / msg's argument
 *	 +-+-------+--------+--- ... ---+
 */

#ifndef _P_BINKP_H_
#define _P_BINKP_H_

#define BINKP_NAME		"binkp"
#define BINKP_MAJOR		1
#define BINKP_MINOR		1
#define BINKP_PORT		24554
#define BINKP_TIMEOUT		(5*60)
#define BINKP_MIN_BLKSIZE 	128
#define BINKP_MAX_BLKSIZE	0x7fff
#define BINKP_DEF_BLKSIZE	(4*1024u)
#define BINKP_BLK_HDRSIZE	2

#define BINKP_MAXPASSWD		64
#define BINKP_MAXSYSTNAME	120
#define BINKP_MAXLOCATION	120
#define BINKP_MAXSYSOP		120
#define BINKP_MAXPHONE		120
#define BINKP_MAXFLAGS		120
#define BINKP_MAXLOCATION	120
#define BINKP_MAXPROGNAME	40
#define BINKP_MAXPROTNAME	40
#define BINKP_MAXTIMESTR	40
#define BINKP_MAXCHALLENGE  128
#define BINKP_MAXOPT        196

#define BINKP_OPT_NR        0x01   /* Non-reliable mode */
#define BINKP_OPT_MB        0x02   /* Multiple batch mode */
#define BINKP_OPT_MPWD      0x04   /* Multiple passwods mode */
#define BINKP_OPT_MD5       0x08   /* CRAM-MD5 authentication */
#define BINKP_OPT_SHA1      0x10   /* CRAM-SHA1 authentication */
#define BINKP_OPT_DES       0x20   /* CRAM-DES authentication */

typedef enum binkp_mode {
  bmode_failoff,
  bmode_incoming_handshake,
  bmode_outgoing_handshake,
  bmode_transfer,
  bmode_complete
} e_binkp_mode;



typedef struct {
	s_sysaddr *addrs;
	int        anum;
	char       passwd[BINKP_MAXPASSWD+1];
	char       systname[BINKP_MAXSYSTNAME+1];
	char       location[BINKP_MAXLOCATION+1];
	char       sysop[BINKP_MAXSYSOP+1];
	char       phone[BINKP_MAXPHONE+1];
	char       flags[BINKP_MAXFLAGS+1];
	char       progname[BINKP_MAXPROGNAME+1];
	char       protname[BINKP_MAXPROTNAME+1];
	char       timestr[BINKP_MAXTIMESTR+1];
	char       opt[BINKP_MAXOPT+1];
	int        majorver;
	int        minorver;
	int        options;
	char       challenge[BINKP_MAXCHALLENGE+1];
	int        challenge_length;
} s_binkp_sysinfo;


typedef enum {
	/*
	 *  Pseudo message types, returned by binkp_recv()
	 */
//	BPMSG_EXIT = -3,	/* Terminate session */
//	BPMSG_NONE = -2,	/* Got nothing intresting */
//	BPMSG_DATA = -1,	/* Got data block */
	/*
	 *  Real BinkP message types
	 */
	BPMSG_NUL = 0,		/* Site information */
	BPMSG_ADR,		/* List of addresses */
	BPMSG_PWD,		/* Session password */
	BPMSG_FILE,		/* File information */
	BPMSG_OK,		/* Password was acknowleged (data ignored) */
	BPMSG_EOB,		/* End Of Batch (data ignored) */
	BPMSG_GOT,		/* File received */
	BPMSG_ERR,		/* Misc errors */
	BPMSG_BSY,		/* All AKAs are busy */
	BPMSG_GET,		/* Get a file from offset */
	BPMSG_SKIP,		/* Skip a file (RECEIVE LATER) */
	BPMSG_MIN = BPMSG_NUL,	/* Minimal message type value */
	BPMSG_MAX = BPMSG_SKIP	/* Maximal message type value */
} e_bpmsg;


//typedef struct {
//	bool sent;		/* Sent message (queued) */
//	e_bpmsg type;		/* Message type */
//	int size;		/* Message size (length of text) */
//	char *data;		/* Message text */
//} s_bpmsg;


//typedef struct {
//	char   *obuf;		/* Output buffer */
//	int     opos;		/* Unsent bytes left in buffer */
//	char   *optr;		/* Send data from this position */
//	
//	char   *ibuf;		/* Our input buffer */
//	int     ipos;
//	int     isize;
////	bool    imsg;		/* Is it message? */
///	bool    inew;
//	e_bpmsg imsgtype;	/* Message type */
  //
//	int     junkcount;
//	s_bpmsg *msgqueue;	/* Outgoing messages queue */
//	int     n_msgs;		/* Number of messages in queue */
//	int	msgs_in_batch;	/* Number of messages in batch */

//	int     timeout;
//} s_bpinfo;

typedef struct {
  char fn[PATH_MAX];
  size_t sz;
  time_t tm;
  size_t offs;
} s_bpfinfo;

/* prot_binkp.c */
int binkp_outgoing(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data);
int binkp_incoming(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data);
int binkp_transfer(s_binkp_sysinfo *local_data, s_binkp_sysinfo *remote_data, s_protinfo *pi);

/* prot_binkp_misc.c */
//void binkp_init_bpinfo(s_bpinfo *bpi);
//void binkp_deinit_bpinfo(s_bpinfo *bpi);
int  binkp_gethdr(const char *s);
int  binkp_getmsgtype(char ch);
void binkp_puthdr(char *s, unsigned int u);
void binkp_putmsgtype(char *s, e_bpmsg msg);
//void binkp_queuemsg(s_bpinfo *bpi, e_bpmsg msg, const char *s1, const char *s2);
//void binkp_queuemsgf(s_bpinfo *bpi, e_bpmsg msg, const char *fmt, ...);
int  binkp_parsfinfo(const char *s, s_bpfinfo *fi, bool with_offset);
//int  binkp_buffer_message(s_bpinfo *bpi, s_bpmsg *msg);
//int  binkp_send_buffer(s_bpinfo *bpi);
//int  binkp_send(s_bpinfo *bpi);
//int  binkp_flush_queue(s_bpinfo *bpi, int timeout);
//int  binkp_recv(s_bpinfo *bpi);
void binkp_update_sysinfo(s_binkp_sysinfo *binkp);
void binkp_log_options(s_binkp_sysinfo *remote);
void binkp_log_sysinfo(s_binkp_sysinfo *binkp);
//void binkp_queue_sysinfo(s_bpinfo *bpi, s_binkp_sysinfo *binkp);
void binkp_set_sysinfo(s_binkp_sysinfo *binkp, s_faddr *remote_addr, bool caller);
void binkp_parse_options(s_binkp_sysinfo *binkp, char *options);

/* prot_binkp_api.c */
extern s_handshake_protocol handshake_protocol_binkp;


#endif /* _P_BINKP_H_ */
