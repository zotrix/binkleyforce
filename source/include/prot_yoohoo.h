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

#ifndef _S_YOOHOO_H_
#define _S_YOOHOO_H_

#define YOOHOO_HELLOLEN   128 /* Size of the 'HELLO' frame */
#define YOOHOO_MAXFIELD   40

/*
 * Information for/from yoohoo handshake (real packet size is 128 bytes)
 */
typedef struct
{
	s_sysaddr *addrs;         /* FTN address */
	int       anum;
	int       product_code;   /* product code */
	int       version_maj;    /* major revision of the product */
	int       version_min;    /* minor revision of the product */
	char      system[YOOHOO_MAXFIELD+1];
	char      sysop[YOOHOO_MAXFIELD+1];
	char      passwd[YOOHOO_MAXFIELD+1];
	int       capabilities;
}
s_yoohoo_sysinfo;

#define YOOHOO_DIETIFNA   0x0001  /* Can do fast "FTS-0001"  */
#define YOOHOO_FTB_USER   0x0002  /* Reserved by Opus-CBCS   */
#define YOOHOO_ZMODEM     0x0004  /* Does ZModem, 1K blocks  */
#define YOOHOO_ZEDZAP     0x0008  /* Can do ZModem variant   */
#define YOOHOO_JANUS      0x0010  /* Can do Janus            */
#define YOOHOO_HYDRA      0x0020  /* Can do Hydra            */
#define YOOHOO_Bit_6      0x0040  /* reserved by FTSC        */
#define YOOHOO_Bit_7      0x0080  /* reserved by FTSC        */
#define YOOHOO_Bit_8      0x0100  /* reserved by FTSC        */
#define YOOHOO_Bit_9      0x0200  /* reserved by FTSC        */
#define YOOHOO_Bit_a      0x0400  /* reserved by FTSC        */
#define YOOHOO_Bit_b      0x0800  /* reserved by FTSC        */
#define YOOHOO_Bit_c      0x1000  /* reserved by FTSC        */
#define YOOHOO_Bit_d      0x2000  /* reserved by FTSC        */
#define YOOHOO_DO_DOMAIN  0x4000  /* Packet contains domain  */
#define YOOHOO_WZ_FREQ    0x8000  /* WZ file req. ok         */

/* prot_yoohoo.c */
int yoohoo_send_hello(s_yoohoo_sysinfo *local_data);
int yoohoo_recv_hello(s_yoohoo_sysinfo *remote_data);
void yoohoo_set_sysinfo(s_yoohoo_sysinfo *local_data, int hrc,
                        e_protocol protocol);
void yoohoo_log_sysinfo(s_yoohoo_sysinfo *yoohoo);

/* prot_yoohoo_api.c */
extern s_handshake_protocol handshake_protocol_yoohoo;

#endif
