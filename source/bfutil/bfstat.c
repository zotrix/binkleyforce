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
#include "version.h"
#include "logger.h"
#include "util.h"
#include "outbound.h"
#include "session.h"

/*
 * Command line options storage structure
 */
typedef struct {
	bool calls_stat;
	bool sort_bysize;
	bool sort_byaddr;
	bool sort_reverse;
	bool disable_total_sizes;
	bool humansizes;
	int nodeslimit;
} s_opts;


/*
 *  Our fake expressions checker. Allways return FALSE (?)
 */
bool eventexpr(s_expr *expr)
{
	return FALSE;
}

static void usage(void)
{
	printf_usage("outbound viewer",
		"usage: bfstat [-afhprst] [-n<number>]\n"
		"\n"
		"options:\n"
		"  -a                sort by FTN address (default)\n"
		"  -c                print incoming/outgoing calls statistic\n"
		"  -f                disable queue sorting\n"
		"  -h                print this help message\n"
		"  -n <number>       don't print more than <number> systems\n"
		"  -p                print sizes in human readable format\n"
		"  -r                reverse order while sorting\n"
		"  -s                sort by total files size\n"
		"  -t                disable total sizes printing\n"
		"\n"
	);
}

static void bfstat_opts_default(s_opts *opts)
{
	memset(opts, '\0', sizeof(s_opts));
	
	opts->sort_bysize   = FALSE;
	opts->sort_byaddr   = TRUE;
	opts->sort_reverse  = FALSE;
	opts->disable_total_sizes = FALSE;
	opts->humansizes    = FALSE;
	opts->nodeslimit    = 0;
}

static void bfstat_print_outbound(s_sysqueue *queue, s_opts *opts)
{
	int i;
	char sbuf[4][10];
	char age_buffer[20];
	char abuf[BF_MAXADDRSTR+1];
	size_t size_netmail = 0;
	size_t size_arcmail = 0;
	size_t size_other   = 0;
	time_t now          = time(0);
	int printnum        = opts->nodeslimit ? MIN(opts->nodeslimit, queue->sysnum)
	                                       : queue->sysnum;
	
	printf("Address                   Netmail   Arcmail   Other     Flavors  Age\n");
	printf("=========================================================================\n");

	for( i = 0; i < printnum; i++ )
	{
		/* Update total size counters */
		if( !opts->disable_total_sizes )
		{
			size_netmail += queue->systab[i].netmail_size;
			size_arcmail += queue->systab[i].arcmail_size;
			size_other   += queue->systab[i].request_size
			              + queue->systab[i].files_size;
		}
		
		/* Get mail age string */
		if( queue->systab[i].mailage > 0 )
			time_string_approx_interval(age_buffer,
					(now - queue->systab[i].mailage));
		else
			*age_buffer = '\0';
		
		if( opts->humansizes )
		{
			printf("%-25s %-9s %-9s %-9s %c%c%c%c%c    %s\n",
				ftn_addrstr(abuf, queue->systab[i].node.addr),
				string_humansize(sbuf[0], queue->systab[i].netmail_size),
				string_humansize(sbuf[1], queue->systab[i].arcmail_size),
				string_humansize(sbuf[2], queue->systab[i].request_size + queue->systab[i].files_size),
				(queue->systab[i].flavors & FLAVOR_HOLD  ) ? 'H' : '.',
				(queue->systab[i].flavors & FLAVOR_NORMAL) ? 'N' : '.',
				(queue->systab[i].flavors & FLAVOR_DIRECT) ? 'D' : '.',
				(queue->systab[i].flavors & FLAVOR_CRASH ) ? 'C' : '.',
				(queue->systab[i].flavors & FLAVOR_IMMED ) ? 'I' : '.',
				age_buffer);
		}
		else
		{
			printf("%-25s %-9ld %-9ld %-9ld %c%c%c%c%c    %s\n",
				ftn_addrstr(abuf, queue->systab[i].node.addr),
				(long)(queue->systab[i].netmail_size),
				(long)(queue->systab[i].arcmail_size),
				(long)(queue->systab[i].request_size + queue->systab[i].files_size),
				(queue->systab[i].flavors & FLAVOR_HOLD  ) ? 'H' : '.',
				(queue->systab[i].flavors & FLAVOR_NORMAL) ? 'N' : '.',
				(queue->systab[i].flavors & FLAVOR_DIRECT) ? 'D' : '.',
				(queue->systab[i].flavors & FLAVOR_CRASH ) ? 'C' : '.',
				(queue->systab[i].flavors & FLAVOR_IMMED ) ? 'I' : '.',
				age_buffer);
		}
	}

	if( !opts->disable_total_sizes )
	{
		printf("=========================================================================\n");
		
		if( opts->humansizes )
		{
			printf("TOTAL %3d systems         %-9s %-9s %-9s %-9s\n",
				printnum,
				string_humansize(sbuf[0], size_netmail),
				string_humansize(sbuf[1], size_arcmail),
				string_humansize(sbuf[2], size_other),
				string_humansize(sbuf[3], size_netmail+size_arcmail+size_other));
		}
		else
		{
			printf("TOTAL %3d systems         %-9ld %-9ld %-9ld %-9ld\n",
				printnum,
				(long)size_netmail, (long)size_arcmail, (long)size_other,
				(long)(size_netmail + size_arcmail + size_other));
		}
	}
}

static void bfstat_print_stat(s_sysqueue *queue, s_opts *opts)
{
	int i;
	s_sess_stat stat;
	char abuf[BF_MAXADDRSTR+1];
	time_t now = time(0);
	int printnum        = opts->nodeslimit ? MIN(opts->nodeslimit, queue->sysnum)
	                                       : queue->sysnum;
	
	printf("Address                   Last incoming   Last outgoing   Status\n");
	printf("=========================================================================\n");
	
	for( i = 0; i < printnum; i++ )
	{
		if( session_stat_get(&stat, &queue->systab[i].node.addr) )
		{
			printf("%-25s No statistic available\n",
				ftn_addrstr(abuf, queue->systab[i].node.addr));
		}
		else
		{
			char last_in[30];
			char last_out[30];
			char status[60];
			char buffer[30];
			
			/*
			 * Format date of the last successful incoming session
			 */
			if( stat.last_success_in > 0 )
			{
				time_string_format(last_in, sizeof(last_in),
						"%H:%M %d/%m/%y", stat.last_success_in);
			}
			else
				strcpy(last_in, "--------------");
			
			/*
			 * Format date of the last successful outgoing session
			 */
			if( stat.last_success_out > 0 )
			{
				time_string_format(last_out, sizeof(last_out),
						"%H:%M %d/%m/%y", stat.last_success_out);
			}
			else
				strcpy(last_out, "--------------");
			
			/*
			 * Generate calls status message
			 */
			if( stat.undialable )
			{
				strcpy(status, "Undialable");
			}
			else if( stat.hold_until > now )
			{
				sprintf(status, "Holded for %s",
						time_string_approx_interval(buffer, stat.hold_until - now));
			}
			else if( stat.hold_freqs > now )
			{
				sprintf(status, "Holded for %s",
						time_string_approx_interval(buffer, stat.hold_freqs - now));
			}
			else
				strcpy(status, "Normal");
			
			if( *status )
			{
				printf("%-25s %-15s %-15s %s\n",
					ftn_addrstr(abuf, queue->systab[i].node.addr),
					last_in, last_out, status);
			}
			else
			{
				printf("%-25s %-15s %-15s\n",
					ftn_addrstr(abuf, queue->systab[i].node.addr),
					last_in, last_out);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	s_opts opts;
	s_sysqueue queue;
	s_outbound_callback_data ocb;
	char c;
	int i;
	
	bfstat_opts_default(&opts);
	
	while( (c = getopt(argc, argv, "acfhn:prst")) != EOF )
	{
		switch( c ) {
		case 'h':
			usage();
			exit(0);
		case 'c':
			opts.calls_stat = TRUE;
			break;
		case 'f':
			opts.sort_bysize = FALSE;
			opts.sort_byaddr = FALSE;
			opts.sort_reverse = FALSE;
			break;
		case 'p':
			opts.humansizes = TRUE;
			break;
		case 't':
			opts.disable_total_sizes = TRUE;
			break;
		case 'r':
			opts.sort_reverse = TRUE;
			break;
		case 's':
			opts.sort_byaddr = FALSE;
			opts.sort_bysize = TRUE;
			break;
		case 'a':
			opts.sort_byaddr = TRUE;
			opts.sort_bysize = FALSE;
			break;
		case 'n':
			if( ISDEC(optarg) )
				opts.nodeslimit = atoi(optarg);
			break;
		default:
			usage();
			exit(1);
		}
	}

	memset(&queue, '\0', sizeof(s_sysqueue));
	
	/* Initialise random number generation */
	(void)srand((unsigned)time(0));
	/* Initialise current locale */
	(void)setlocale(LC_ALL, "");
	
	if( conf_readconf(conf_getconfname(), 0) )
		exit(1);

	memset(&ocb, '\0', sizeof(s_outbound_callback_data));
	ocb.callback = out_handle_sysqueue;
	ocb.dest = (void *)&queue;
	out_scan(&ocb, NULL);
	
	if( queue.sysnum > 1 && (opts.sort_byaddr || opts.sort_bysize) )
	{
		int sort_opts = opts.sort_byaddr ? QUEUE_SORT_ADDRESS
		              : opts.sort_bysize ? QUEUE_SORT_SIZE : 0;
		if( opts.sort_reverse )
			sort_opts |= QUEUE_SORT_REVERSE;
		out_sysqueue_sort(&queue, sort_opts);
	}

	if( queue.sysnum > 0 )
	{
		if( opts.calls_stat )
			bfstat_print_stat(&queue, &opts);
		else
			bfstat_print_outbound(&queue, &opts);
	}
	else
		printf("Outbound queue is empty\n");
	
	fflush(stdout);
	
	deinit_sysqueue(&queue);
	deinit_conf();

	exit(0);
}

