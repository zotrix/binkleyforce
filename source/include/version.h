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

#ifndef _VERSION_H_
#define _VERSION_H_


#define BF_PRODCODE   0xfe
#define BF_NAME       "binkleyforce"
#define BF_RDATE      "n/a"
#define BF_VERSION    RELEASE_VERSION
#define BF_CDATE      __DATE__ __TIME__
#define BF_REG        "free software"
#define BF_COPYRIGHT  "(c) 1997-2000 by Alexander Belkin"

#define BF_BANNERVER  BF_NAME" "BF_VERSION"/"BF_OS

#define BF_EMSI_NUM   "fe"
#define BF_EMSI_NAME  BF_NAME
#define BF_EMSI_VER   BF_VERSION"/"BF_OS
#define BF_EMSI_REG   BF_REG

#endif
