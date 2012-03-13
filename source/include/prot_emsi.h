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

#ifndef _S_EMSI_H_
#define _S_EMSI_H_

/*
 * EMSI limits
 */
#define EMSI_MAXDAT        16384
#define EMSI_MAXNFLAG      20
#define EMSI_MAXPASSWD     30
#define EMSI_MAXMPID       30
#define EMSI_MAXMNAME      120
#define EMSI_MAXMVER       120
#define EMSI_MAXMREG       120
#define EMSI_MAXSYSNAME    120
#define EMSI_MAXLOCATION   120
#define EMSI_MAXSYSOP      120
#define EMSI_MAXPHONE      120
#define EMSI_MAXFLAGS      120
#define EMSI_MAXOHTIME     120
#define EMSI_MAXFRTIME     120
#define EMSI_MAXXDATETIME  60
#define EMSI_MAXADDONS     120

/*
 * EMSI XXn flags that affect only address number n in AKA list
 */
#define EMSI_FLAG_PU       0x0001 /* Pickup FILES for this address               */
#define EMSI_FLAG_HA       0x0002 /* Hold all FILES for this address             */
#define EMSI_FLAG_PM       0x0004 /* PickUp Mail ONLY for this address           */
#define EMSI_FLAG_NF       0x0008 /* No TIC'S, associated files or file attaches */
                                  /* for this address                            */
#define EMSI_FLAG_NX       0x0010 /* No compressed mail pickup desired,          */
                                  /* for this address                            */
#define EMSI_FLAG_NR       0x0020 /* File requests not accepted by caller        */
                                  /* for this address                            */
#define EMSI_FLAG_HN       0x0040 /* Hold all traffic EXCEPT Mail (ARCmail and   */
                                  /* Packets) for this address                   */
#define EMSI_FLAG_HX       0x0080 /* Hold compressed mail for this address       */
#define EMSI_FLAG_HF       0x0100 /* Hold tic's and associated files and file    */
                                  /* attaches other than mail for this address   */
#define EMSI_FLAG_HR       0x0200 /* Hold file requests (not processed at this   */
                                  /* time)for this address                       */

/*
 *  Defines for state machine
 */
#define	SME		-1
#define	SM0		0
#define	SM1		1
#define	SM2		2
#define	SM3		3
#define	SM4		4
#define	SM5		5
#define	SM6		6
#define	SM7		7
#define	SM8		8
#define	SM9		9

struct states
{
const	char *st_name;
	int (*proc)();
};
typedef struct states s_states;

struct linkcodes
{
	/*
	 * EMSI-I (FSC-0056) and EMSI-II (FSC-0088) flags
	 * for XXn flags see s_emsiaddr structure
	 */
	 	
	UINT32 N81:1,   /* Communication parameter emulation :) (EMSI-I)     */
	       PUP:1,   /* Pickup FILES for primary address only             */
	       PUA:1,   /* Pickup FILES for all presented addresses          */	
	       NPU:1,   /* No FILE pickup desired. (calling system)          */
	       HAT:1,   /* Hold all FILES          (answering system)        */
	       PMO:1,   /* PickUp Mail (ARCmail and Packets) ONLY            */
	       NFE:1,   /* No TIC'S, associated files or files attaches      */
	                /* desired                                           */
	       NXP:1,   /* No compressed mail pickup desired                 */
	       NRQ:1,   /* File requests not accepted by caller              */
	                /* This flag is presented if file request processing */
	                /* is disabled TEMPORARILY for any reason            */
	       HNM:1,   /* Hold all traffic EXCEPT Mail(ARCmail and Packets) */
	       HXT:1,   /* Hold compressed mail traffic.                     */
	       HFE:1,   /* Hold tic's and associated files                   */
	                /* and file attaches other than mail                 */
	       HRQ:1,   /* Hold file requests (not processed at this time)   */
	                /* this flag is presented if file request processing */
	                /* is disabled TEMPORARILY for any reason            */
	       FNC:1,   /* Convert file names to "msdos" format              */
	       RMA:1,   /* System is able to process multiple file requests  */
	       RH1:1;   /* Under Hydra batch 1 contain file requests only,   */
	                /* while batch 2 is reserved for all other files     */
};
typedef struct linkcodes s_linkcodes;

struct compcodes
{
	UINT16 NCP:1,   /* No compatible protocols (failure) */
	       ZMO:1,   /* Zmodem w/1,024 byte data packets  */
	       ZAP:1,   /* ZedZap    (Zmodem variant)        */
	       DZA:1,   /* DirectZAP (Zmodem variant)        */
	       JAN:1,   /* Janus                             */
	       HYD:1;   /* Hydra                             */

	UINT16 FRQ:1,   /* The system will accept and process FREQs */
	       NRQ:1,   /* No file requests accepted by this system */
	       ARC:1,   /* ARCmail 0.60-capable                     */
	       XMA:1,   /* Supports other forms of compressed mail  */
	       FNC:1,   /* Filename conversion into 8.3 format      */
	       EII:1,   /* EMSI-II flags are supported              */
	       DFB:1,   /* Can fall-back to FTS1/WAZOO negotiation  */
	       CHT:1,   /* Chat during transmittion available       */
	       BBS:1,   /* Site has public BBS available            */
	       HFR:1;   /* Remote will automaticaly hold requested  */
	                /* files for us                             */ 
};
typedef struct compcodes s_compcodes;

struct emsi
{
	/*
	 * Keep here all unknown EMSI_DAT addons
	 */
	char        addons[EMSI_MAXADDONS+1];
	
	/*
	 * {EMSI_DAT} (main handshake information)
	 */ 
	bool        have_emsi;
//	s_sysaddr  *addrs;		 /* dynamicaly allocated array       */
//	int         anum;		 /* number of used entries in it     */
	char        passwd[EMSI_MAXPASSWD+1];
	s_linkcodes linkcodes;		/* XXn linkcodes contained in eaddr  */
	s_compcodes compcodes;
	char        m_pid[EMSI_MAXMPID+1];
	char        m_name[EMSI_MAXMNAME+1];
	char        m_ver[EMSI_MAXMVER+1];
	char        m_reg[EMSI_MAXMREG+1];

	/*
	 * {IDENT} (system information)
	 */
	bool        have_ident;
	char        sname[EMSI_MAXSYSNAME+1];
	char        location[EMSI_MAXLOCATION+1];
	char        sysop[EMSI_MAXSYSOP+1];
	char        phone[EMSI_MAXPHONE+1];
	int         speed;
	char        flags[EMSI_MAXFLAGS+1];
	
	/*
	 * {TRAF} (size of netmail and arcmail files on hold)
	 */
	bool        have_traf;
	size_t      netmail_size;
	size_t      arcmail_size;
	
	/*
	 * {MOH#} (size of files on hold? :))
	 */
	bool        have_moh;
	size_t      files_size;
	
	/*
	 * {OHFR} (site's work and freq time intervals)
	 */
	bool        have_ohfr;
	char        oh_time[EMSI_MAXOHTIME+1];
	char        fr_time[EMSI_MAXFRTIME+1];
	
	/*
	 * {TZUTC} (site's time zone)
	 */
	bool        have_tzutc;
	int         tzutc;
	
	/*
	 * {XDATETIME} (site's date and time in extended format)
	 */
	bool        have_xdatetime;
	char        xdatetime[EMSI_MAXXDATETIME+1];
	
	/*
	 * {TRX#} (site's current time)
	 */
	bool        have_trx;
	time_t      time;
};
typedef struct emsi s_emsi;

/* prot_emsi.c */
int emsi_send_emsidat(s_emsi *local_emsi);
int emsi_recv_emsidat(s_emsi *remote_emsi);
void emsi_set_sysinfo(s_emsi *emsi, s_emsi *remote_emsi, int hrc,
                      e_protocol protocol);

/* prot_emsi_api.c */
extern s_handshake_protocol handshake_protocol_emsi;

/* prot_emsi_misc.c */
char *emsi_createdat(s_emsi *emsi);
int   emsi_parsedat(char *emsi_dat, s_emsi *emsi);
void  emsi_logdat(s_emsi *emsi);

#endif
