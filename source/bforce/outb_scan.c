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

s_exttab outtab[] =
{
	{ ".iut", TYPE_NETMAIL, FLAVOR_IMMED,  ACTION_UNLINK   },
	{ ".cut", TYPE_NETMAIL, FLAVOR_CRASH,  ACTION_UNLINK   },
	{ ".dut", TYPE_NETMAIL, FLAVOR_DIRECT, ACTION_UNLINK   },
	{ ".out", TYPE_NETMAIL, FLAVOR_NORMAL, ACTION_UNLINK   },
	{ ".hut", TYPE_NETMAIL, FLAVOR_HOLD,   ACTION_UNLINK   },
	{ ".ikt", TYPE_NETMAIL, FLAVOR_IMMED,  ACTION_UNLINK   },
	{ ".ckt", TYPE_NETMAIL, FLAVOR_CRASH,  ACTION_UNLINK   },
	{ ".dkt", TYPE_NETMAIL, FLAVOR_DIRECT, ACTION_UNLINK   },
	{ ".pkt", TYPE_NETMAIL, FLAVOR_NORMAL, ACTION_UNLINK   },
	{ ".hkt", TYPE_NETMAIL, FLAVOR_HOLD,   ACTION_UNLINK   },
	{ ".ilo", TYPE_FLOFILE, FLAVOR_IMMED,  ACTION_NOTHING  },
	{ ".clo", TYPE_FLOFILE, FLAVOR_CRASH,  ACTION_NOTHING  },
	{ ".dlo", TYPE_FLOFILE, FLAVOR_DIRECT, ACTION_NOTHING  },
	{ ".flo", TYPE_FLOFILE, FLAVOR_NORMAL, ACTION_NOTHING  },
	{ ".hlo", TYPE_FLOFILE, FLAVOR_HOLD,   ACTION_NOTHING  },
	{ ".req", TYPE_REQUEST, FLAVOR_NONE,   ACTION_NOTHING  },
	{  NULL,  0,            0,             0               }
};
	
s_exttab exttab[] =
{
	{ ".tic", TYPE_TICFILE, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".su?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".mo?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".tu?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".we?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".th?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".fr?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{ ".sa?", TYPE_ARCMAIL, FLAVOR_NORMAL, ACTION_TRUNCATE },
	{  NULL,  0,            0,             0               }
};

/*****************************************************************************
 * Devide full outbound path on "root" directory, there other oubound
 * directories created, and main outbound directory name
 *
 * Arguments:
 *   path       pointer to the null-terminated outbound path string
 *   out_root   pointer to the pointer for the resulting root directory
 *   out_main   pointer to the pointer for the resulting main outbound
 *              name w/o path
 *
 * Return value:
 *   Zero on success (*out_root and *out_main must be freed)
 */
int out_get_root(const char *path, char **out_root, char **out_main)
{
	char *p, *outb;

	ASSERT(path != NULL && out_root != NULL && out_main != NULL);

	*out_root = NULL;
	*out_main = NULL;
	
	outb = (char *)xstrcpy(path);

	p = outb + strlen(outb) - 1;
	if( *p == DIRSEPCHR )
		*p = '\0';
	
	/* Ohh.. where is no full path for outbound directory */
	if( (p=strrchr(outb, DIRSEPCHR)) == NULL )
	{
		free(outb);
		return -1;
	}
	
	*p = '\0';
	*out_main = xstrcpy(p+1);
	*out_root = xstrcat(outb, "/");
	
	DEB((D_OUTBOUND, "out_getroot: out_root \"%s\", out_main \"%s\"",
		*out_root, *out_main));
		
	return 0;
}

/*****************************************************************************
 * Scan Binkley Style Outbound directory, for each file for requested
 * addresses call callback function
 *
 * Arguments:
 *   callback   pointer to the callback information structure
 *   fa_list    pointer to the requested addresses list
 *   addr       current address (most time it is partially filled)
 *   path       path for the outbound to scan
 *   point      point number for the current outbound
 *
 * Return value:
 *   Zero on success
 */
static int out_scan_bso_dir(s_outbound_callback_data *callback,
		const s_falist *fa_list, s_faddr *addr, const char *path, int point)
{
	DIR *dir;
	struct dirent *dirent;
	char *p;
	const s_falist *alst;

	ASSERT(callback);
	ASSERT(callback->dest);
	ASSERT(addr && path);
	
	DEB((D_OUTBOUND, "out_scan_bso_dir: BSO: scan dir \"%s\"", path));
	
	if( (dir=opendir(path)) == NULL )
	{
		logerr("can't open outbound directory \"%s\"", path);
		return(1);
	}
	
	while( (dirent=readdir(dir)) != NULL )
	{
		if( strlen(dirent->d_name) == 12 &&
		    strspn(dirent->d_name, "0123456789abcdefABCDEF") == 8 )
		{
			if((strncasecmp(dirent->d_name+8, ".PNT", 4) != 0) &&
			  ( strncasecmp(dirent->d_name+8, ".REQ", 4) != 0) &&
			  ( strncasecmp(dirent->d_name+10,  "UT", 2) != 0) &&
			  ( strncasecmp(dirent->d_name+10,  "LO", 2) != 0) ) continue;
			if( point == 0 && strncasecmp(dirent->d_name+8, ".PNT", 4) == 0 )
			{
				DEB((D_OUTBOUND, "scan_dir: BSO: found point directory \"%s\"",
					dirent->d_name));
					
				/*
				 * Congratulation, we found point's direcory
				 */
				sscanf(dirent->d_name, "%04x%04x", &addr->net, &addr->node);
				for( alst = fa_list; alst; alst = alst->next )
					if( addr->net  == alst->addr.net
					 && addr->node == alst->addr.node )
						break;
				
				if( alst || !fa_list )
				{
					p = string_concat(path, dirent->d_name, "/", NULL);
					out_scan_bso_dir(callback, fa_list, addr, p, 1);
					if( p ) { free(p); p = NULL; }
				}
			}
			else
			{
				if( point )
				{
					sscanf(dirent->d_name, "%08x", &addr->point);
				}
				else
				{
					addr->point = 0;
					sscanf(dirent->d_name, "%04x%04x", &addr->net, &addr->node);
				}

				/* Check, does we really need in this file? */
				for( alst = fa_list; alst && ftn_addrcomp(*addr, alst->addr);
				     alst = alst->next );
				
				if( alst || !fa_list )
				{
					callback->path = string_concat(path, dirent->d_name, NULL);
					callback->addr = *addr;
					callback->type = OUTB_TYPE_BSO;
					callback->flavor = -1;
					callback->callback(callback);
					if( callback->path )
						free(callback->path);
				}
			}
		}
	}
	
	closedir(dir);
	
	return(0);
}

static int out_scan_bso(s_outbound_callback_data *callback,
		const s_falist *mailfor, const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	const s_falist *alst;
	s_faddr addr;
	char *newpath;
	char *out_root = NULL;	/* Root for outbound directories tree */
	char *out_main = NULL;	/* Main outbound directory name       */
	int len;
	
	ASSERT(path);

	if( out_get_root(path, &out_root, &out_main) )
		return -1;
	
	ASSERT(out_root && out_main);
	
	if( (dir = opendir(out_root)) == NULL )
	{
		logerr("can't open outbound directory \"%s\"", out_root);
		free(out_root); free(out_main);
		return -1;
	}
	
	/* Scan outbound (only 4D) */
	while( (dirent = readdir(dir)) )
	{
		memset(&addr, '\0', sizeof(s_faddr));
		if( strcmp(dirent->d_name, out_main) == 0 )
		{
			/* Our main outbound */
			s_cval_entry *cfptr = conf_first(cf_address);
			if( cfptr )
				addr.zone = cfptr->d.falist.addr.zone;
			else
				addr.zone = 666;
		}
		else if( strncmp(dirent->d_name, out_main, len=strlen(out_main)) == 0
		      && dirent->d_name[len] == '.'
		      && strspn(dirent->d_name+len+1,"0123456789abcdefABCDEF") == 3 )
		{
			/* Ooutbound with zone ext. (like /outbound.308/) */
			sscanf(dirent->d_name+len+1, "%03x", &addr.zone);
		}

		if( addr.zone )
		{
			for( alst = mailfor;
				alst && addr.zone != alst->addr.zone;
				alst = alst->next );
		
			if( alst || mailfor == NULL )
			{
				newpath = string_concat(out_root, dirent->d_name, "/", NULL);
				out_scan_bso_dir(callback, mailfor, &addr, newpath, 0);
				free(newpath);
			}
		}
	}

	closedir(dir);
	
	free(out_root);
	free(out_main);

	return 0;
}

static int out_parse_name_lbox(s_faddr *addr, const char *filename)
{
	s_faddr tmp;
	
	ASSERT(addr && filename);
	
	memset(&tmp, '\0', sizeof(s_faddr));
	if( sscanf(filename, "%d.%d.%d.%d",
		&tmp.zone, &tmp.net, &tmp.node, &tmp.point) == 4 )
	{
		*addr = tmp;
		return 0;
	}
	
	return -1;
}

static int out_scan_fbox_dir(s_outbound_callback_data *callback,
		const char *path, s_faddr addr, int flavor)
{
	DIR *dir;
	struct dirent *dirent;
	
	if( (dir = opendir(path)) == NULL )
	{
		logerr("can't open filebox directory \"%s\"", path);
		return -1;
	}
	
	while( (dirent = readdir(dir)) )
	{
		callback->path = string_concat(path, dirent->d_name, NULL);
		if( is_regfile(callback->path) )
		{
			callback->addr = addr;
			callback->type = OUTB_TYPE_FBOX;
			callback->flavor = flavor;
			callback->callback(callback);
		}
		free(callback->path);
		callback->path = NULL;
	}
	
	closedir(dir);
	
	return 0;
}

static int out_scan_lbox(s_outbound_callback_data *callback,
		const s_falist *mailfor, const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	const s_falist *alst;
	s_faddr addr;
	char *newpath;
	
	if( (dir = opendir(path)) == NULL )
	{
		logerr("can't open filebox directory \"%s\"", path);
		return -1;
	}
	
	while( (dirent = readdir(dir)) )
	{
		if( out_parse_name_lbox(&addr, dirent->d_name) == 0 )
		{
			if( mailfor )
			{
				/* Scan only this fileboxes */
				for( alst = mailfor;
					alst && ftn_addrcomp(addr, alst->addr);
					alst = alst->next );
				if( alst )
				{
					newpath = string_concat(path, dirent->d_name, "/", NULL);
					(void)out_scan_fbox_dir(callback, newpath, addr, FLAVOR_HOLD);
					free(newpath);					
				}
			}
			else
			{
				/* Scan all fileboxes */
				newpath = string_concat(path, dirent->d_name, "/", NULL);
				(void)out_scan_fbox_dir(callback, newpath, addr, FLAVOR_HOLD);
				free(newpath);
			}
		}
	}
	
	closedir(dir);
	
	return 0;
}

/*****************************************************************************
 * Parse file name from AmigaDos Style Outbound
 *
 * Arguments:
 *   name       pointer to the null-terminated file name
 *   addr       pointer to the memory where to store resulting address
 *
 * Return value:
 *   Zero on success
 */
int out_parse_name_aso(s_faddr *addr, const char *filename)
{
	s_faddr tmp;
	
	ASSERT(addr && filename);
	
	memset(&tmp, '\0', sizeof(s_faddr));
	if( sscanf(filename, "%d.%d.%d.%d.%*s", &tmp.zone,
	    &tmp.net, &tmp.node, &tmp.point) == 4 )
	{
		*addr = tmp;
		return 0;
	}
	
	return -1;
}

static int out_scan_aso(s_outbound_callback_data *callback,
		const s_falist *mailfor, const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	const s_falist *alst;
	s_faddr addr = { FALSE, 0, 0, 0, 0, "" };
#ifdef DEBUG
	char abuf[BF_MAXADDRSTR+1];
#endif
		
	ASSERT(callback);
	ASSERT(callback->dest);
	ASSERT(path);

	DEB((D_OUTBOUND, "out_scandir_aso: scan dir \"%s\"", path));
	
	if( (dir = opendir(path)) == NULL )
	{
		logerr("can't open outbound directory \"%s\"", path);
		return -1;
	}
	
	while( (dirent = readdir(dir)) )
	{
		memset(&addr, '\0', sizeof(s_faddr));
		
		if( out_parse_name_aso(&addr, dirent->d_name) == 0 )
		{
			DEB((D_OUTBOUND, "out_scandir_aso: file \"%s\" for address %s",
				dirent->d_name, ftn_addrstr(abuf, addr)));

			for( alst = mailfor; alst && ftn_addrcomp(addr, alst->addr);
			     alst = alst->next );
			
			if( alst || !mailfor )
			{
				callback->path = string_concat(path, dirent->d_name, NULL);
				callback->addr = addr;
				callback->type = OUTB_TYPE_ASO;
				callback->flavor = -1;
				callback->callback(callback);
				if( callback->path )
					free(callback->path);
			}
		}
	}
	
	closedir(dir);
	
	return 0;
}

/* ------------------------------------------------------------------------- */
/* Scan outbound for addresses fa_list, use handler proc                     */
/* ------------------------------------------------------------------------- */
int out_scan(s_outbound_callback_data *callback, const s_falist *fa_list)
{
	const s_falist *alst;
	s_faddr addr = { FALSE, 0, 0, 0, 0, "" };
	s_cval_entry *cfptr;
	char *path;
	
	ASSERT(callback);
	ASSERT(callback->callback);
	ASSERT(callback->dest);

	/*
	 * 1st - try out 4D outbound
	 */
	path = conf_string(cf_outbound_directory);
	if( path && *path )
		(void)out_scan_bso(callback, fa_list, path);
	
	/*
	 *  2th - Scan domain outbounds
	 */
	for( cfptr = conf_first(cf_domain); cfptr; cfptr = conf_next(cfptr) )
	{
		memset(&addr, '\0', sizeof(s_faddr));
		for( alst = fa_list; alst; alst = alst->next )
		{
			if( cfptr->d.domain.zone == alst->addr.zone
			 || alst->addr.zone == -1 )
			{
				addr.zone = cfptr->d.domain.zone;
				out_scan_bso_dir(callback, fa_list, &addr,
						cfptr->d.domain.path, 0);
				break;
			}
		}
	}

	/*
	 *  3th - Scan AmigaDOS outbound
	 */
	path = conf_string(cf_amiga_outbound_directory);
	if( path && *path )
		(void)out_scan_aso(callback, fa_list, path);

	/*
	 * 4th - Scan personal fileboxes
	 */
	if( fa_list )
	{
		/* Scan only listed fileboxes */
		for( cfptr = conf_first(cf_filebox); cfptr; cfptr = conf_next(cfptr) )
		{
			for( alst = fa_list; alst; alst = alst->next )
			{
				if( !ftn_addrcomp(alst->addr, cfptr->d.filebox.addr) )
					out_scan_fbox_dir(callback, cfptr->d.filebox.path,
							cfptr->d.filebox.addr, cfptr->d.filebox.flavor);
			}
		}
	}
	else
	{
		/* Scan all fileboxes */
		for( cfptr = conf_first(cf_filebox); cfptr; cfptr = conf_next(cfptr) )
			out_scan_fbox_dir(callback, cfptr->d.filebox.path,
					cfptr->d.filebox.addr, cfptr->d.filebox.flavor);
	}
	
	/*
	 * 5th - Scan "long" fileboxes
	 */
	path = conf_string(cf_filebox_directory);
	if( path && *path )
		(void)out_scan_lbox(callback, fa_list, path);

	return(0);
}
