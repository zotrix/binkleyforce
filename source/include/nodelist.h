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

#ifndef _NODELIST_H_
#define _NODELIST_H_

/*
 * Each nodelist has corresponding index file. Index file name is the
 * same as nodelist, but extension is changed to the ".bni", so index
 * file to the nodelist "net5020.ndl" will be called "net5020.bni"
 *
 * Nodelist index header:
 *
 *  Offset   Size       Description
 *  -------------------------------
 *  0x0000   4 bytes    Nodelist file date
 *  0x0004   4 bytes    Nodelist file size
 *  0x0008   4 bytes    Total number of entries in index file
 *  0x000c   4 bytes    Unused
 *
 * Nodelist index is an array of next entries:
 *
 *  Offset   Size       Description
 *  -------------------------------
 *  0x0010   2 bytes    Zone number
 *  0x0012   2 bytes    Net number
 *  0x0014   2 bytes    Node number
 *  0x0016   2 bytes    Point number
 *  0x0018   2 bytes    HUB number (I hope HUB is in the same zone:net)
 *  0x001a   4 bytes    Offset of string in nodelist file
 *
 */

enum
{
	NODELIST_HDRSIZE = 16,		/* Don't change! */
	NODELIST_ENTRYSIZE = 14,	/* Don't change! */
	NODELIST_READAHEAD = 50
};

/*
 *  Modes for nodelist_opendindex()
 */
enum nodelist_iomode
{
	NODELIST_READ,
	NODELIST_WRITE
};

/*
 *  Positions of <...> in nodelist strings
 */
enum nodelist_positon
{
	NODELIST_POSKEYWORD = 0,
	NODELIST_POSNUMBER = 1,
	NODELIST_POSNAME = 2,
	NODELIST_POSLOCATION = 3,
	NODELIST_POSSYSOP = 4,
	NODELIST_POSPHONE = 5,
	NODELIST_POSSPEED = 6,
	NODELIST_POSFLAGS = 7
};

/*
 * Storage sizes for node structure
 */
enum nodelist_limit
{
	BNI_MAXNAME = 48,
	BNI_MAXLOCATION = 48,
	BNI_MAXSYSOP = 48,
	BNI_MAXPHONE = 48,
	BNI_MAXFLAGS = 120
};

/*
 * node->keyword values
 */
enum nodelist_keyword
{
	KEYWORD_EMPTY,
	KEYWORD_ZONE,
	KEYWORD_REGION,
	KEYWORD_HOST,
	KEYWORD_HUB,
	KEYWORD_PVT,
	KEYWORD_HOLD,
	KEYWORD_DOWN,
	KEYWORD_BOSS,
	KEYWORD_POINT
};

/*
 * Nodelist procedures error codes
 */
enum nodelist_error
{
	BNIERR_NOERROR = 0,
	BNIERR_NODENOTFOUND = 1,
	BNIERR_IDXIOERR = 2,
	BNIERR_NDLIOERR = 3,
	BNIERR_BADINDEX = 4,
	BNIERR_NONODELIST = 5
};

typedef struct nodelist
{
	FILE *fp_index;
	FILE *fp_nodelist;
	char name_index[BF_MAXPATH+1];
	char name_nodelist[BF_MAXPATH+1];
	long entries;
}
s_nodelist;

typedef struct bni
{
	int zone;
	int net;
	int node;
	int point;
	int hub;
	long offset;
}
s_bni;

typedef struct node
{
	s_faddr addr;
	s_faddr addr_hub;
	enum nodelist_keyword keyword;
	bool listed;
	char name[BNI_MAXNAME+1];
	char location[BNI_MAXLOCATION+1];
	char sysop[BNI_MAXSYSOP+1];
	char phone[BNI_MAXPHONE+1];
	long speed;
	char flags[BNI_MAXFLAGS+1];
	s_timevec worktime;
}
s_node;

int   nodelist_checkflag(const char *nodeflags, const char *flag);
int   nodelist_keywordval(const char *keyword);
int   nodelist_parsestring(s_node *node, char *str);
s_nodelist *nodelist_open(const char *dir, const char *name, int mode);
int   nodelist_checkheader(s_nodelist *nlp);
int   nodelist_createheader(s_nodelist *nlp);
int   nodelist_close(s_nodelist *nlp);
int   nodelist_putindex(s_nodelist *nlp, const s_bni *bni);
int   nodelist_findindex(s_nodelist *nlp, s_bni *bni, s_faddr addr);
int   nodelist_getstr(s_nodelist *nlp, size_t offset, char *buffer, size_t buflen);
int   nodelist_lookup_string(char *buffer, size_t buflen, s_faddr addr);
int   nodelist_lookup(s_node *node, s_faddr addr);
void  nodelist_initnode(s_node *node, s_faddr addr);

#endif /* _NODELIST_H_ */
