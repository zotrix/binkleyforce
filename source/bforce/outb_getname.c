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
#include "outbound.h"

char *out_getname_4d(s_faddr addr)
{
	s_cval_entry *cfptr = NULL;
	char *p_outbound = NULL;
	char *out_root = NULL;
	char *out_main = NULL;
	char *dest = NULL;
	char buf[128];
	s_faddr mainaddr;
	
	p_outbound = conf_string(cf_outbound_directory);

	if( (cfptr = conf_first(cf_address)) )
		mainaddr = cfptr->d.falist.addr;
	else
		memset(&mainaddr, '\0', sizeof(s_faddr));

	if( p_outbound && *p_outbound && cfptr
	 && out_get_root(p_outbound, &out_root, &out_main) == 0
	 && out_root && *out_root && out_main && *out_main )
	{
		if( addr.zone == mainaddr.zone )
		{
			/* It is our primary outbound */
			if( addr.point == 0 )
				sprintf(buf, "%04x%04x", addr.net, addr.node);
			else
				sprintf(buf, "%04x%04x.pnt/%08x", addr.net, addr.node, addr.point);
			
			dest = string_concat(p_outbound, buf, NULL);
		}
		else
		{
			if( addr.point == 0 )
				sprintf(buf, ".%03x/%04x%04x", addr.zone, addr.net, addr.node);
			else
				sprintf(buf, ".%03x/%04x%04x.pnt/%08x", addr.zone, addr.net, addr.node, addr.point);

			dest = string_concat(out_root, out_main, buf, NULL);
		}
	}
	
	if( out_root ) free(out_root);
	if( out_main ) free(out_main);
	
	return(dest);
}

char *out_getname_domain(s_faddr addr)
{
	s_cval_entry *cfptr;
	char buf[128];
	char *dest = NULL;

	/* find corresponding domain outbound.. */
	for( cfptr = conf_first(cf_domain); cfptr;
	     cfptr = conf_next(cfptr) )
	{
		if( cfptr->d.domain.zone == addr.zone )
		{
			/* first of all get file name */
			if( addr.point == 0 )
				sprintf(buf, "%04x%04x", addr.net, addr.node);
			else
				sprintf(buf, "%04x%04x.pnt/%08x", addr.net, addr.node, addr.point);
			
			dest = string_concat(cfptr->d.domain.path, buf, NULL);
			break;
		}
	}
	
	return dest;
}

char *out_getname_amiga(s_faddr addr)
{
	char buf[128];
	char *p_amigaoutbound = NULL;
	char *dest = NULL;
	
	p_amigaoutbound = conf_string(cf_amiga_outbound_directory);
	
	if( p_amigaoutbound && *p_amigaoutbound )
	{
		sprintf(buf, "%d.%d.%d.%d", addr.zone, addr.net, addr.node, addr.point);
		dest = string_concat(p_amigaoutbound, buf, NULL);
	}
	
	return dest;
}

/* end */
