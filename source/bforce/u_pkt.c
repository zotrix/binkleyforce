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

/*
 * Packet header offsets
 */
#define PKTHDR_ORIGNODE       0
#define PKTHDR_DESTNODE       2
#define PKTHDR_YEAR           4
#define PKTHDR_MONTH          6
#define PKTHDR_DAY            8
#define PKTHDR_HOUR           10
#define PKTHDR_MINUTE         12
#define PKTHDR_SECOND         14
#define PKTHDR_BAUD           16
#define PKTHDR_PKTTYPE        18
#define PKTHDR_ORIGNET        20
#define PKTHDR_DESTNET        22
#define PKTHDR_PRODCODELOW    24
#define PKTHDR_REVMAJOR       25
#define PKTHDR_PASSWORD       26
#define PKTHDR_QORIGZONE      34
#define PKTHDR_QDESTZONE      36
#define PKTHDR_AUXNET         38
#define PKTHDR_CWVALIDCOPY    40
#define PKTHDR_PRODCODEHIGH   42
#define PKTHDR_REVMINOR       43
#define PKTHDR_CAPABILWORD    44
#define PKTHDR_ORIGZONE       46
#define PKTHDR_DESTZONE       48
#define PKTHDR_ORIGPOINT      50
#define PKTHDR_DESTPOINT      52
#define PKTHDR_PRODDATA       54

#define PKTHDR_SIZE           58

/*
 * Packed message header offsets
 */
#define PKTMSGHDR_PKTTYPE      0
#define PKTMSGHDR_ORIGNODE     2
#define PKTMSGHDR_DESTNODE     4
#define PKTMSGHDR_ORIGNET      6
#define PKTMSGHDR_DESTNET      8
#define PKTMSGHDR_ATTR        10
#define PKTMSGHDR_COST        12
#define PKTMSGHDR_DATE        14 /* 20 bytes */
#define PKTMSGHDR_TOUSER      24 /* 36 bytes */
#define PKTMSGHDR_FROMUSER    60 /* 36 bytes */
#define PKTMSGHDR_SUBJECT     96 /* 72 bytes */

#define PKTMSGHDR_SIZE        168

/*
 * Message attributes
 */
#define MSGATTR_PVT             0x0001
#define MSGATTR_CRASH           0x0002
#define MSGATTR_RECD            0x0004
#define MSGATTR_SENT            0x0008
#define MSGATTR_FILEATTACH      0x0010
#define MSGATTR_INTRANSIT       0x0020
#define MSGATTR_ORPHAN          0x0040
#define MSGATTR_KILLSENT        0x0080
#define MSGATTR_LOCAL           0x0100
#define MSGATTR_HOLD            0x0200
#define MSGATTR_RESERVED        0x0400
#define MSGATTR_FILEREQ         0x0800
#define MSGATTR_RREQ            0x1000
#define MSGATTR_IRRR            0x2000
#define MSGATTR_AUDIT           0x4000
#define MSGATTR_UPDATEREQ       0x8000
#define MSGATTR_NOTZEROED       ( MSGATTR_PVT | MSGATTR_CRASH \
                                | MSGATTR_FILEATTACH | MSGATTR_RESERVED \
                                | MSGATTR_RREQ | MSGATTR_IRRR | MSGATTR_AUDIT )

#include "includes.h"
#include "confread.h"
#include "logger.h"
#include "util.h"

static char *pkt_putword(char *buf, int val)
{
	buf[0] = ( ((unsigned int) val)       ) & 0xff;
	buf[1] = ( ((unsigned int) val) >> 8  ) & 0xff;
	
	return buf;
}

static char *pkt_putbyte(char *buf, int val)
{
	buf[0] = ( ((unsigned int) val)       ) & 0xff;
	
	return buf;
}

static int pkt_getword(const char *buf)
{
	return ( (unsigned int) ((unsigned char) buf[0])      )
	     | ( (unsigned int) ((unsigned char) buf[1]) << 8 );
}

static char *pkt_putpktheader(char *pkthdr_buffer, const s_packet *pkt)
{
	time_t now = localtogmt(time(NULL));
	struct tm *ptm = localtime(&now);

	memset(pkthdr_buffer, '\0', PKTHDR_SIZE);
	
	pkt_putword(pkthdr_buffer + PKTHDR_ORIGNODE, pkt->orig.node);
	pkt_putword(pkthdr_buffer + PKTHDR_DESTNODE, pkt->dest.node);
	pkt_putword(pkthdr_buffer + PKTHDR_YEAR, ptm->tm_year);
	pkt_putword(pkthdr_buffer + PKTHDR_MONTH, ptm->tm_mon);
	pkt_putword(pkthdr_buffer + PKTHDR_DAY, ptm->tm_mday);
	pkt_putword(pkthdr_buffer + PKTHDR_HOUR, ptm->tm_hour);
	pkt_putword(pkthdr_buffer + PKTHDR_MINUTE, ptm->tm_min);
	pkt_putword(pkthdr_buffer + PKTHDR_SECOND, ptm->tm_sec);
	pkt_putword(pkthdr_buffer + PKTHDR_BAUD, pkt->baud);
	pkt_putword(pkthdr_buffer + PKTHDR_PKTTYPE, 0x0002);
	pkt_putword(pkthdr_buffer + PKTHDR_ORIGNET, pkt->orig.net);
	pkt_putword(pkthdr_buffer + PKTHDR_DESTNET, pkt->dest.net);
	pkt_putbyte(pkthdr_buffer + PKTHDR_PRODCODELOW, 0xfe);
	pkt_putword(pkthdr_buffer + PKTHDR_REVMAJOR, 0);
	memcpy(pkthdr_buffer + PKTHDR_PASSWORD, pkt->password, MIN(8, strlen(pkt->password)));
	pkt_putword(pkthdr_buffer + PKTHDR_QORIGZONE, pkt->orig.zone);
	pkt_putword(pkthdr_buffer + PKTHDR_QDESTZONE, pkt->dest.zone);
	/* .. skipped .. */
	pkt_putword(pkthdr_buffer + PKTHDR_ORIGZONE, pkt->orig.zone);
	pkt_putword(pkthdr_buffer + PKTHDR_DESTZONE, pkt->dest.zone);
	pkt_putword(pkthdr_buffer + PKTHDR_ORIGPOINT, pkt->orig.point);
	pkt_putword(pkthdr_buffer + PKTHDR_DESTPOINT, pkt->dest.point);
	
	return pkthdr_buffer;
}

static char *pkt_putmsgheader(char *msghdr_buffer, const s_message *msg)
{
	char buf[30];
	
	memset(msghdr_buffer, '\0', PKTMSGHDR_SIZE);
	
	pkt_putword(msghdr_buffer + PKTMSGHDR_PKTTYPE, 0x0002);
	pkt_putword(msghdr_buffer + PKTMSGHDR_ORIGNODE, msg->orig.node);
	pkt_putword(msghdr_buffer + PKTMSGHDR_DESTNODE, msg->dest.node);
	pkt_putword(msghdr_buffer + PKTMSGHDR_ORIGNET, msg->orig.net);
	pkt_putword(msghdr_buffer + PKTMSGHDR_DESTNET, msg->dest.net);
	pkt_putword(msghdr_buffer + PKTMSGHDR_ATTR, msg->attr & MSGATTR_NOTZEROED);
	pkt_putword(msghdr_buffer + PKTMSGHDR_COST, msg->cost);
	strnxcpy(msghdr_buffer + PKTMSGHDR_DATE, time_string_msghdr(buf, msg->time), 20);
	strnxcpy(msghdr_buffer + PKTMSGHDR_TOUSER, msg->nameto, 36);
	strnxcpy(msghdr_buffer + PKTMSGHDR_FROMUSER, msg->namefrom, 36);
	strnxcpy(msghdr_buffer + PKTMSGHDR_SUBJECT, msg->subject, 72);

	return msghdr_buffer;
}

static int pkt_writepacket(FILE *fp, const s_packet *pkt)
{
	int i;
	char pkthdr[PKTHDR_SIZE];
	char msghdr[PKTMSGHDR_SIZE];
	
	pkt_putpktheader(pkthdr, pkt);
	
	if( fwrite(pkthdr, sizeof(pkthdr), 1, fp) != 1 )
		return 1;
	
	for( i = 0; i < pkt->n_msgs; i++ )
	{
		pkt_putmsgheader(msghdr, &pkt->msgs[i]);
		
		if( fwrite(msghdr, sizeof(msghdr), 1, fp) != 1 )
			return 1;
		
		if( pkt->msgs[i].text )
			fprintf(fp, "%s\r", pkt->msgs[i].text);
		
		if( pkt->msgs[i].tagline )
			fprintf(fp, "... %s\r", pkt->msgs[i].tagline);

		fprintf(fp, "--- %s\r",
			pkt->msgs[i].tearline ? pkt->msgs[i].tearline : BF_BANNERVER);
		
		if( pkt->msgs[i].origin )
			fprintf(fp, "* Origin: %s\r", pkt->msgs[i].origin);
		
		fputc('\0', fp); /* end of message */
	}

	fputc('\0', fp); /* end of packet */
	fputc('\0', fp); /* end of packet */
	
	return 0;
}

int pkt_createpacket(const char *pktname, const s_packet *pkt)
{
	int rc;
	FILE *fp;
	
	if( (fp = file_open(pktname, "w")) == NULL )
		return -1;
	
	rc = pkt_writepacket(fp, pkt);

	if( file_close(fp) )
		return -1;
	
	return rc;
}
