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

#ifndef _UTIL_H_
#define _UTIL_H_

#define DEFAULT_ZONE    2
#define DEFAULT_NET     5020
#define DEFAULT_NODE    0

#define TIMEVEC_MAX_ELEMENTS	10

enum { ADDR_EQ, ADDR_GT, ADDR_LT };

enum {
	PLOCK_OK,
	PLOCK_EXIST,
	PLOCK_ERROR,
	PLOCK_OURLOCK
};

enum day
{
	DAY_MONDAY,
	DAY_TUESDAY,
	DAY_WEDNESDAY,
	DAY_THURSDAY,
	DAY_FRIDAY,
	DAY_SATURDAY,
	DAY_SUNDAY,
	DAY_ANY,
	DAY_WORKDAY,
	DAY_WEEKEND,
	DAY_UNDEF
};

typedef struct faddr {
	bool inetform;	/* Is address in domain form? */
	int zone;		/* -1 value means any?! */
	int net;
	int node;
	int point;
	char domain[BF_MAXDOMAIN+1];
} s_faddr;

typedef struct timeint {
	enum day day_beg;
	enum day day_end;
	long beg;
	long end;
} s_timeint;

typedef struct timevec {
	int num;
	s_timeint tvec[TIMEVEC_MAX_ELEMENTS];
} s_timevec;

typedef struct message {
	int     attr;
	int     cost;
	time_t  time;
	s_faddr orig;
	s_faddr dest;
	char    namefrom[36];
	char    nameto[36];
	char    subject[72];
	char   *text;
	char   *tagline;
	char   *tearline;
	char   *origin;
} s_message;

typedef struct packet {
	s_faddr orig;
	s_faddr dest;
	int     baud;
	char    password[8+1];
	s_message *msgs;
	int     n_msgs;
} s_packet;

enum
{
	FILELOCK_CLEAR,
	FILELOCK_READ,
	FILELOCK_WRITE
};

#define RECODE_MAX_CHAR 255

typedef struct recode_table {
	const char *filename;
	char table[RECODE_MAX_CHAR+1];
} s_recode_table;

#undef  MAX
#define MAX(a, b)           (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)           (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)              (((a) < 0) ? -(a) : (a))

#define SET_LAT_A_Z         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define SET_LAT_a_z         "abcdefghijklmnopqrstuvwxyz"
#define SET_DEC             "0123456789"
#define SET_HEX             "0123456789ABCDEFabcdef"
#define SET_OCT             "01234567"

#define CRC16TEST(crc)      ((crc) == 0xf0b8)
#define CRC32TEST(crc)      ((crc) == 0xdebb20e3L)

#define updcrc16(cp, crc)   (TableCRC16Xmodem[(((int)(crc) >> 8) & 0xff)] ^ ((crc) << 8) ^ (cp))
#define updcrc32(cp, crc)   (TableCRC32CCITT[((int)(crc) ^ cp) & 0xff] ^ (((crc) >> 8) & 0x00FFFFFF))

#define localtogmt(t)       ((t) - time_gmtoffset()*60)
#define gmttolocal(t)       ((t) + time_gmtoffset()*60)

#define timer_set(ptr, v)   (time_settimer(ptr, v))
#define timer_expired(tm)   (time(NULL) > (tm))
#define timer_running(tm)   ((tm) > 0)
#define timer_timeleft(tm)  (time_timeleft(tm))

#define recode_file_out(str) recode(str, &recode_table_out, conf_string(cf_recode_file_out));
#define recode_file_in(str)  recode(str, &recode_table_in, conf_string(cf_recode_file_in));
#define recode_intro_in(str) recode(str, &recode_table_in, conf_string(cf_recode_intro_in));

extern s_recode_table recode_table_in;
extern s_recode_table recode_table_out;
extern unsigned long  TableCRC32CCITT[];
extern unsigned short TableCRC16Xmodem[];
extern unsigned short TableCRC16CCITT[];

/* u_crc.c */
unsigned long  getcrc32ccitt(const unsigned char *buffer, size_t buflen);
unsigned short getcrc16xmodem(const unsigned char *buffer, size_t buflen);
unsigned short getcrc16ccitt(const unsigned char *buffer, size_t buflen);

/* u_file.c */
int   file_lock(FILE *fp, bool exclusive);
void  file_unlock(FILE *fp);
int   file_lock_wait(FILE *fp, bool exclusive);
bool  file_name_issafe(int ch);
char *file_name_makesafe(char *filename);
char *file_getname(char *filename);
char *file_gettmp(void);
char *file_get_dos_name(char *buffer, const char *filename);
bool  is_directory(const char *dirname);
bool  is_regfile(const char *filename);
int   directory_create(const char *dirname, mode_t access_mode);
FILE *file_open(const char *path, const char *mode);
int   file_close(FILE *stream);

/* u_ftn.c */
int   ftn_addrparse(s_faddr *addr, const char *s, bool wildcard);
char *ftn_addrstr_fido(char *buf, s_faddr addr);
char *ftn_addrstr_inet(char *buf, s_faddr addr);
char *ftn_addrstr(char *buf, s_faddr addr);
int   ftn_addrcomp(s_faddr addr1, s_faddr addr2);
int   ftn_addrcomp_logic(s_faddr addr1, int operator, s_faddr addr2);
int   ftn_addrcomp_mask(s_faddr addr, s_faddr mask);
int   ftn_addrsmetric(s_faddr addr1, s_faddr addr2);

/* u_md5.c */
void md5_get(const unsigned char *data, size_t length, unsigned char *digest);
void md5_cram_get(const unsigned char *secret, const unsigned char *challenge,
                  int challenge_length, unsigned char *digest);

/* u_misc.c */
void *xmalloc(size_t size);
void *xrealloc(void *buf, size_t size);
void *xmemcpy(const void *buffer, size_t buflen);
char *strparse(char *str, char **next, int quoted);
int   strcasemask(const char *str, const char *mask);
int   checkmasks(const char *masks, const char *str);
char *buffer_putlong(char *buffer, long val);
char *buffer_putint(char *buf, int val);
long  buffer_getlong(const char *buf);
int   buffer_getint(const char *buf);
void  printf_usage(const char *ident, const char *fmt, ...);

/* u_pkt.c */
int   pkt_createpacket(const char *pktname, const s_packet *pkt);

/* u_plock.c */
int   plock_check(const char *lockname);
int   plock_create(const char *lockname);
int   plock_remove(const char *lockname);
int   plock_link(const char *lockname, const char *tmpname);
		
/* u_recode.c */
char *recode(char *src, s_recode_table *tab, const char *filename);

/* u_string.c */
char       *xstrcpy(const char *src);
char       *xstrcat(char *src, const char *add);
char       *strnxcpy(char *dst, const char *src, size_t len);
char       *strnxcat(char *dst, const char *src, size_t len);
char       *string_token(char *str, char **next, const char *delim, int quoted);
char       *string_chomp(char *str);
const char *string_casestr(const char *string, const char *substr);
const char *string_casechr(const char *str, int ch);
char       *string_toupper(char *str);
char       *string_tolower(char *str);
bool        string_isupper(const char *str);
bool        string_islower(const char *str);
char       *string_trimright(char *str);
char       *string_trimleft(char *str);
char       *string_trimboth(char *str);
char       *string_printable_buffer(const char *buffer, size_t buflen);
const char *string_printable(const char *str);
char       *string_replchar(char *str, char oldchar, char newchar);
int         string_parse(char **dest, int items, char *str, int separator);
int         string_parse_regular(char **dest, int items, char *str);
char       *string_translate(const char *str, const char *find, const char *repl);
char       *string_humansize(char *buffer, size_t size);
int         string_get_escape(char **pptr);
int         string_dequote(char *dst, char *src);
char       *string_concat(const char *str, ...);
void        string_bin_to_hex(char *string, const char *binptr, int binlen);
int         string_hex_to_bin(char *binptr, const char *string);
bool        string_is_empty(const char *string);

/* u_time.c */
int   time_settimer(time_t *timer, int value);
int   time_timerout(time_t timer);
int   time_timeleft(time_t timer);
int   time_elapsed(time_t since_time);
char *time_string_format(char *buffer, size_t buflen, const char *fmt, time_t t);
char *time_string_long(char *buffer, size_t buflen, time_t t);
char *time_string_log(char *buffer, size_t buflen, time_t t);
char *time_string_msghdr(char *buffer, time_t t);
long  time_gmtoffset(void);
char *time_string_gmtoffset(char *buffer);
char *time_string_timer(char *buffer, int timer);
char *time_string_approx_interval(char *buffer, int seconds);
int   time_check(const char *str, struct tm *now);
int   time_checkintervals(const char *timestr, struct tm *now);
int   timevec_add(s_timevec *dest, int day_beg, int day_end, long beg, long end);
int   timevec_parse(s_timevec *dest, const char *str);
int   timevec_parse_list(s_timevec *dest, const char *str);
int   timevec_check(const s_timevec *tv, const struct tm *now);
char *timevec_string(char *buffer, const s_timevec *tv, size_t buflen);
bool  timevec_isdefined(const s_timevec *tv);
bool  timevec_isnow(const s_timevec *tv, const struct tm *now);

#endif /* _UTIL_H_ */
