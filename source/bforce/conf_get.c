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

s_cval_entry *conf_first(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl;

	for( ptrl = bforce_config[keyword].data; ptrl; ptrl = ptrl->next )
		if( !ptrl->expr.expr || eventexpr(&ptrl->expr) )
			return ptrl;
	
	return NULL;
}

s_cval_entry *conf_next(s_cval_entry *ptrl)
{
	ASSERT(ptrl);
	
	for( ptrl = ptrl->next; ptrl; ptrl = ptrl->next )
		if( !ptrl->expr.expr || eventexpr(&ptrl->expr) )
			return ptrl;
	
	return NULL;
}

bool conf_boolean(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl = conf_first(keyword);

	return ptrl ? ptrl->d.boolean.istrue : FALSE;
}

mode_t conf_filemode(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl = conf_first(keyword);

	return ptrl ? ptrl->d.filemode.mode : (mode_t)0;
}

long conf_options(bforce_config_keyword keyword)
{
	long result = 0L;
	s_cval_entry *ptrl;
	
	for( ptrl = conf_first(keyword); ptrl; ptrl = conf_next(ptrl) )
		result = ((~ptrl->d.options.mask) & result) | ptrl->d.options.value;
	
	return result;
}

long conf_connlist(bforce_config_keyword keyword, long speed)
{
	s_cval_entry *ptrl;

	for( ptrl = conf_first(keyword); ptrl; ptrl = conf_next(ptrl) )
		if( ptrl->d.connlist.speed == speed )
			return ptrl->d.connlist.value;
	
	return 0;
}

char *conf_string(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl = conf_first(keyword);

	return ptrl ? ptrl->d.string.str : (char*)NULL;
}

long conf_number(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl = conf_first(keyword);

	return ptrl ? ptrl->d.number.num : (long)0L;
}

s_override *conf_override(bforce_config_keyword keyword, s_faddr addr)
{
	s_cval_entry *ptrl;

	for( ptrl = conf_first(keyword); ptrl; ptrl = conf_next(ptrl) )
		if( !ftn_addrcomp(ptrl->d.override.addr, addr) )
			return &ptrl->d.override;
	
	return NULL;
}

s_tries *conf_tries(bforce_config_keyword keyword)
{
	s_cval_entry *ptrl = conf_first(keyword);

	return ptrl ? &ptrl->d.tries : NULL;
}

