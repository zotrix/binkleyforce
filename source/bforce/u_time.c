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

static const char *months[] =
{
	"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

int time_settimer(time_t *timer, int value)
{
	*timer = time(NULL) + value;
	
	return 0;
}

int time_timerout(time_t timer)
{
	if( timer > 0 && time(NULL) >= timer )
		return 1;
	
	return 0;
}

int time_timeleft(time_t timer)
{
	if( timer > 0 )
		return MAX(time(NULL) - timer, 0);
	
	return 0;
}

int time_elapsed(time_t since_time)
{
	if( since_time > 0 )
	{
		int yield = time(NULL) - since_time;
		return ( yield > 0 ) ? yield : 0;
	}
	
	return 0;
}

/*****************************************************************************
 * Put time string into buffer, according to the format
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	buflen    buffer size
 * 	fmt       format string (`man strftime' for more information)
 * 	t         seconds since 1970.. you know..
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_format(char *buffer, size_t buflen, const char *fmt, time_t t)
{
	struct tm *tm;
	
	if( !t ) t = time(&t);
	
	tm = localtime(&t);
	
	if( strftime(buffer, buflen, fmt, tm) == 0 )
	{
		strnxcpy(buffer, "invalid format", buflen);
	}
	
	return buffer;
}

/*****************************************************************************
 * Put time string into buffer (time format depends on the locale settings).
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	buflen    buffer size
 * 	t         seconds since 1970.. you know..
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_long(char *buffer, size_t buflen, time_t t)
{
	char *sptr;
	
	if( !t ) time(&t);
	
	sptr = ctime(&t);
	strnxcpy(buffer, sptr, buflen);
	
	return string_chomp(buffer);
}

/*****************************************************************************
 * Put time string into buffer (e.g. My birth day is "Aug 11 07:20:00").
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	buflen    buffer size
 * 	t         seconds since 1970.. you know..
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_log(char *buffer, size_t buflen, time_t t)
{
	struct tm *ptm;

	ASSERT(buflen > 16);
	
	if( !t ) time(&t);
	
	ptm = localtime(&t);
	sprintf(buffer, "%3s %02d %02d:%02d:%02d",
		months[ptm->tm_mon], ptm->tm_mday,
		ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		
	return buffer;
}

/*****************************************************************************
 * Put date+time string into buffer. Special format for netmail message
 * headers.
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer
 * 	buflen    buffer size
 * 	t         seconds since 1970.. you know..
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_msghdr(char *buffer, time_t t)
{
	struct tm *ptm;

	if( !t ) time(&t);

	ptm = localtime(&t);
	sprintf(buffer, "%02d %3s %02d  %02d:%02d:%02d",
		ptm->tm_mday, months[ptm->tm_mon],
		(ptm->tm_year < 100) ? ptm->tm_year : ptm->tm_year % 100,
		ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	
	return buffer;
}

/*****************************************************************************
 * Get TimeZone for our location
 *
 * Arguments:
 *
 * Return value:
 * 	Offset in minutes
 */
long time_gmtoffset(void)
{
	time_t tt       = time(NULL);
	struct tm local = *localtime(&tt);
	struct tm gmt   = *gmtime(&tt);
	long tz         = gmt.tm_yday - local.tm_yday;
	
	if( tz > 1 )
		tz = -24;
	else if( tz < -1 )
		tz = 24;
	else
		tz *= 24;
	
	tz += gmt.tm_hour - local.tm_hour;
	tz *= 60;
	tz += gmt.tm_min - local.tm_min;
	
	return tz;
}

/*****************************************************************************
 * Get TimeZone for our location as pointer to the null terminated string
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer (must be at least 6 bytes)
 *
 * Return value:
 * 	Pointer to the resulting string (e.g. "+0400")
 */
char *time_string_gmtoffset(char *buffer)
{
	long tz   = time_gmtoffset();
	char sign = (tz > 0)?'+':'-';
	int hour  = tz/60;
	int mint  = tz%60;
	
	sprintf(buffer, "%c%02d%02d", sign, hour, mint);
	
	return buffer;
}

/*****************************************************************************
 * Get time in native format from seconds counter (e.g. "1:30:10")
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer (must be at least 9 bytes)
 * 	timer     number of seconds
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_timer(char *buffer, int timer)
{
	int hour, min, sec;
	
	hour = (timer/3600);
	min  = (timer%3600)/60;
	sec  = (timer%60);
	
	if( hour )
		sprintf(buffer, "%02d:%02d:%02d", (hour < 100) ? hour : 99, min, sec);
	else
		sprintf(buffer, "%02d:%02d", min, sec);
	
	return buffer;
}

/*****************************************************************************
 * Get approximate interval in native format (e.g. "1 day", "3 hours")
 *
 * Arguments:
 * 	buffer    pointer to the destination buffer (must be large enough)
 * 	seconds   time interval in seconds
 *
 * Return value:
 * 	Pointer to the resulting string
 */
char *time_string_approx_interval(char *buffer, int seconds)
{
	if( seconds < 0 )
		strcpy(buffer, "error");
	else if( seconds == 0 )
		strcpy(buffer, "up-to-date");
	else if( seconds == 1 )
		strcpy(buffer, "1 second");
	else if( seconds < 60 )
		sprintf(buffer, "%d seconds", seconds);
	else if( seconds < 120 )
		strcpy(buffer, "1 minute");
	else if( seconds < 3600 )
		sprintf(buffer, "%d minutes", seconds/60);
	else if( seconds < 7200 )
		strcpy(buffer, "1 hour");
	else if( seconds < 86400 )
		sprintf(buffer, "%d hours", seconds/3600);
	else if( seconds < 172800 )
		strcpy(buffer, "1 day");
	else if( seconds < 999*86400 )
		sprintf(buffer, "%d days", seconds/86400);
	else
		sprintf(buffer, "%d years", seconds/(366*86400));
	
	return buffer;
}

/*****************************************************************************
 * Check time for conforming to the time interval specified by string
 *
 * Arguments:
 * 	str       pointer to the time interval string
 * 	now       time to check (most often it is current time)
 *
 * Return value:
 * 	Zero value if time conforms, 1 if not, and -1 if specified time interval
 * 	string is incorrect
 */
int time_check(const char *str, struct tm *now)
{
	int h1, h2, m1, m2, beg, end, cur, day;
	bool dayok  = FALSE;
	bool timeok = FALSE;

	ASSERT(str != NULL && now != NULL);
	
	if( *str == '\0' ) return 1;
	
	     if( strncasecmp(str, "Sun", 3) == 0 ) day = 0;
	else if( strncasecmp(str, "Mon", 3) == 0 ) day = 1;
	else if( strncasecmp(str, "Tue", 3) == 0 ) day = 2;
	else if( strncasecmp(str, "Wed", 3) == 0 ) day = 3;
	else if( strncasecmp(str, "Thu", 3) == 0 ) day = 4;
	else if( strncasecmp(str, "Fri", 3) == 0 ) day = 5;
	else if( strncasecmp(str, "Sat", 3) == 0 ) day = 6;
	else if( strncasecmp(str, "Any", 3) == 0 ) day = -1;
	else if( strncasecmp(str, "Wk",  2) == 0 ) day = -2;
	else if( strncasecmp(str, "We",  2) == 0 ) day = -3;
	else day = -4;

	if( day >= 0 )
	{
		dayok = (now->tm_wday == day);
		str += 3;
	}
	else
	{
		switch(day) {
		case -3:
			dayok = ( (now->tm_wday == 0) || (now->tm_wday == 6) );
			str += 2;
			break;
		case -2:
			dayok = ( (now->tm_wday != 0) && (now->tm_wday != 6) );
			str += 2;
			break;
		case -1:
			str += 3;
		default:
			dayok = TRUE;
		}
	}

	if( !dayok ) return 1;
	
	if( *str == '\0' )
		return (day == -4) ? -1 : 0;
	
	if( sscanf(str, "%02d:%02d-%02d:%02d", &h1, &m1, &h2, &m2) != 4 )
		return -1;
	
	cur = now->tm_hour*60 + now->tm_min;
	beg = h1*60 + m1;
	end = h2*60 + m2;
	
	if( end > beg )
		timeok = ( (cur >= beg) && (cur <= end) );
	else
		timeok = ( (cur >= beg) || (cur <= end) );

	DEB((D_INFO, "checktime: is %02d:%02d between %02d:%02d and %02d:%02d? %s!",
		now->tm_hour,now->tm_min, h1, m1, h2, m2, timeok ? "Yes" : "No"));
	
 	return !timeok;
}

/*****************************************************************************
 * Check time for conforming to the time intervals
 *
 * Arguments:
 * 	timestr   pointer to the time intervals string
 * 	tim       time to check (most often it is current time)
 *
 * Return value:
 * 	Zero value if time conforms, 1 if not, and -1 if specified time interval
 * 	string is incorrect
 */
int time_checkintervals(const char *timestr, struct tm *now)
{
	char *str, *p;

	ASSERT(timestr != NULL);
	
	str = xstrcpy(timestr);
	
	for( p = strtok(str, ","); p; p = strtok(NULL, ",") )
	{
		if( *p )
		{
			switch(time_check(p, now)) {
			case  0:
				return 0;
			case -1:
				return -1;
			case  1:
				break;
			default:
				ASSERT(0);
			}
		}
	}
	
	return 1;
}

int timevec_add(s_timevec *dest, int day_beg, int day_end, long beg, long end)
{
	int i;
	
	/*
	 * Check for dupes. TODO: more complex checks
	 */
	for( i = 0; i < dest->num; i++ )
	{
			if( dest->tvec[i].day_beg <= day_beg
			 && dest->tvec[i].day_end >= day_end
			 && dest->tvec[i].beg == beg
			 && dest->tvec[i].end == end )
				return 0;
	}
	
	if( dest->num >= TIMEVEC_MAX_ELEMENTS )
		return -1;

	dest->tvec[dest->num].day_beg = day_beg;
	dest->tvec[dest->num].day_end = day_end;
	dest->tvec[dest->num].beg = beg;
	dest->tvec[dest->num].end = end;
	dest->num++;

	return 0;
}

/*****************************************************************************
 * Parse one time interval. Only uucico like time format is supported yet.
 *
 * Arguments:
 * 	dest      pointer to the destination time storage structure
 * 	str       time string to parse
 *
 * Return value:
 * 	Zero value on success, and -1 for incorrect time strings
 */
int timevec_parse(s_timevec *dest, const char *str)
{
	int beg_hour = 0;
	int end_hour = 0;
	int beg_min  = 0;
	int end_min  = 0;
	enum day beg_day;
	enum day end_day;
	int beg = 0;
	int end = 0;

	if( strncasecmp(str, "Sun", 3) == 0 )
		beg_day = DAY_SUNDAY;
	else if( strncasecmp(str, "Mon", 3) == 0 )
		beg_day = DAY_MONDAY;
	else if( strncasecmp(str, "Tue", 3) == 0 )
		beg_day = DAY_TUESDAY;
	else if( strncasecmp(str, "Wed", 3) == 0 )
		beg_day = DAY_WEDNESDAY;
	else if( strncasecmp(str, "Thu", 3) == 0 )
		beg_day = DAY_THURSDAY;
	else if( strncasecmp(str, "Fri", 3) == 0 )
		beg_day = DAY_FRIDAY;
	else if( strncasecmp(str, "Sat", 3) == 0 )
		beg_day = DAY_SATURDAY;
	else if( strncasecmp(str, "Any", 3) == 0 )
		beg_day = DAY_ANY;
	else if( strncasecmp(str, "Wk",  2) == 0 )
		beg_day = DAY_WORKDAY;
	else if( strncasecmp(str, "We",  2) == 0 )
		beg_day = DAY_WEEKEND;
	else
		beg_day = DAY_UNDEF;
	
	end_day = beg_day;
	
	if( beg_day >= DAY_MONDAY && beg_day <= DAY_SUNDAY )
		str += 3;
	else if( beg_day == DAY_ANY )
		str += 3;
	else if( beg_day == DAY_WORKDAY || beg_day == DAY_WEEKEND )
		str += 2;
	
	if( sscanf(str, "%02d:%02d-%02d:%02d", &beg_hour, &beg_min, &end_hour, &end_min) != 4 )
		return -1;

	beg = beg_hour * 60 + beg_min;
	end = end_hour * 60 + end_min;

	if( beg == end )
		return -1;
	
	switch(beg_day) {
	case DAY_MONDAY:
	case DAY_TUESDAY:
	case DAY_WEDNESDAY:
	case DAY_THURSDAY:
	case DAY_FRIDAY:
	case DAY_SATURDAY:
	case DAY_SUNDAY:
		timevec_add(dest, beg_day, end_day, beg, end);
		break;
	case DAY_WORKDAY:
		timevec_add(dest, DAY_MONDAY, DAY_FRIDAY, beg, end);
		break;
	case DAY_WEEKEND:
		timevec_add(dest, DAY_SATURDAY, DAY_SUNDAY, beg, end);
		break;
	default:
		timevec_add(dest, DAY_MONDAY, DAY_SUNDAY, beg, end);
		break;
	}
		
	return 0;
}

int timevec_parse_list(s_timevec *dest, const char *str)
{
	char *p = NULL;
	char *tmp = xstrcpy(str);
	int rc = 0;
	
	for( p = strtok(tmp, ","); p; p = strtok(NULL, ",") )
	{
		if( *p && timevec_parse(dest, p) == -1 )
			rc = -1;
	}

	free(tmp);

	return rc;
}

int timevec_check(const s_timevec *tv, const struct tm *now)
{
	int i;
	bool ok = FALSE;
	int cur = now->tm_hour * 60 + now->tm_min;
	int cur_day = DAY_UNDEF;

	if( now->tm_wday == 0 )
		cur_day = DAY_SUNDAY;
	else if( now->tm_wday == 1 )
		cur_day = DAY_MONDAY;
	else if( now->tm_wday == 2 )
		cur_day = DAY_TUESDAY;
	else if( now->tm_wday == 3 )
		cur_day = DAY_WEDNESDAY;
	else if( now->tm_wday == 4 )
		cur_day = DAY_THURSDAY;
	else if( now->tm_wday == 5 )
		cur_day = DAY_FRIDAY;
	else if( now->tm_wday == 6 )
		cur_day = DAY_SATURDAY;
	else
	{
		log("timevec_check: error 'tm->tm_wday' is out of range");
		return -1;
	}
	
	for( i = 0; i < tv->num; i++ )
	{
		if( (cur_day >= tv->tvec[i].day_beg)
		 && (cur_day <= tv->tvec[i].day_end) )
		{
			int beg = tv->tvec[i].beg;
			int end = tv->tvec[i].end;
			
			if( end > beg )
				ok = ( (cur >= beg) && (cur <= end) );
			else
				ok = ( (cur >= beg) || (cur <= end) );
			
			if( ok ) break;
		}
	}
	
	return ok ? 0 : 1;
}

char *timevec_string(char *buffer, const s_timevec *tv, size_t buflen)
{
	int i;
	char *ptr = buffer;
	
	*ptr = '\0';
	
	for( i = 0; i < tv->num; i++ )
	{
		if( tv->tvec[i].day_beg == DAY_MONDAY
		 && tv->tvec[i].day_end == DAY_SUNDAY )
		{
			sprintf(ptr, "%02ld:%02ld-%02ld:%02ld,",
				tv->tvec[i].beg / 60, tv->tvec[i].beg % 60,
				tv->tvec[i].end / 60, tv->tvec[i].end % 60);
		}
		else if( tv->tvec[i].day_beg == tv->tvec[i].day_end )
		{
			sprintf(ptr, "%d.%02ld:%02ld-%02ld:%02ld,", tv->tvec[i].day_beg,
				tv->tvec[i].beg / 60, tv->tvec[i].beg % 60,
				tv->tvec[i].end / 60, tv->tvec[i].end % 60);
		}
		else
		{
			sprintf(ptr, "%d.%02ld:%02ld-%d.%02ld:%02ld,",
				tv->tvec[i].day_beg, tv->tvec[i].beg / 60, tv->tvec[i].beg % 60,
				tv->tvec[i].day_end, tv->tvec[i].end / 60, tv->tvec[i].end % 60);
		}
		
		ptr += strlen(ptr);
	}

	if( ptr > buffer )
		*(ptr - 1) = '\0';

	return buffer;
}

bool timevec_isdefined(const s_timevec *tv)
{
	return tv->num ? TRUE : FALSE;
}

bool timevec_isnow(const s_timevec *tv, const struct tm *now)
{
	return ( timevec_check(tv, now) == 0 ) ? TRUE : FALSE;
}

/* end of u_time.c */
