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


void deinit_conf(void)
{
	s_cval_entry *ptrl;
	s_cval_entry *next;
	int i;
	
	for( i = 0; bforce_config[i].name; i++ )
	{
		for( ptrl = bforce_config[i].data; ptrl; ptrl = next )
		{
			next = ptrl->next;
			deinit_cval_entry(ptrl, bforce_config[i].type);
			free(ptrl);
		}
		bforce_config[i].data = NULL;
	}
}

void deinit_cval_entry(s_cval_entry *dest, bforce_config_keyword type)
{
	/*
	 * Other config variables doesn't use allocated memory
	 */
	switch(type) {
	case CT_ADDRESS:
	case CT_NODELIST:
	case CT_PASSWORD:
		if( dest->d.falist.what )
			free(dest->d.falist.what);
		break;
	case CT_DOMAIN:
		deinit_domain(&dest->d.domain);
		break;
	case CT_DIALRESP:
		deinit_dialresp(&dest->d.dialresp);
		break;
	case CT_MODEMPORT:
		deinit_modemport(&dest->d.modemport);
		break;
	case CT_OVERRIDE:
		deinit_override(&dest->d.override);
		break;
	case CT_PATH:
	case CT_STRING:
		deinit_string(&dest->d.string);
		break;
	case CT_TRANSLATE:
		deinit_translate(&dest->d.translate);
		break;
	default:
		DEB((D_CONFIG, "deinit_cval_entry: type %d doesn't use dynamic memory",
				type));
	}
}

void deinit_dialresp(s_dialresp *dest)
{
	if( dest->mstr ) free(dest->mstr);
}

void deinit_domain(s_domain *dest)
{
	if( dest->domain ) free(dest->domain);
	if( dest->path ) free(dest->path);
}

void deinit_expr(s_expr *dest)
{
	if( dest->expr ) free(dest->expr);
}

void deinit_falist(s_falist *dest)
{
	s_falist *ptrl, *next;
	
	for( ptrl = dest; ptrl; ptrl = next )
	{
		next = ptrl->next;
		if( ptrl->what ) free(ptrl->what);
		free(ptrl);
	}
}

void deinit_modemport(s_modemport *dest)
{
	if( dest->name ) free(dest->name);
}

void deinit_override(s_override *dest)
{
	s_override *ptrl, *next;
	
	if( dest->sIpaddr ) free(dest->sIpaddr);
	if( dest->sPhone  ) free(dest->sPhone);
	if( dest->sFlags  ) free(dest->sFlags);
	for( ptrl = dest->hidden; ptrl; ptrl = next )
	{
		next = ptrl->hidden;
		if( ptrl->sIpaddr ) free(ptrl->sIpaddr);
		if( ptrl->sPhone  ) free(ptrl->sPhone);
		if( ptrl->sFlags  ) free(ptrl->sFlags);
		free(ptrl);
	}
}

void deinit_string(s_string *dest)
{
	if( dest->str ) free(dest->str);
}

void deinit_translate(s_translate *dest)
{
	if( dest->find ) free(dest->find);
	if( dest->repl ) free(dest->repl);
}

