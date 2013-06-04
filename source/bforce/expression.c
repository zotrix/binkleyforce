#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20120115

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)

#define YYPREFIX "yy"

#define YYPURE 0

#line 49 "bforce/expression.y"
#include "includes.h"
#include "confread.h"
#include "util.h"
#include "logger.h"
#include "session.h"
#include "nodelist.h"
#include "io.h"

static struct tm *now    = NULL;
static int   expr_result = 0;
static char *expr_p_pos  = NULL;
static char *expr_p_text = NULL;

/*
 * These are expression element checkers (not only)
 * Return values:
 *    0  -  FALSE
 *    1  -  TRUE
 *   -1  -  cannot check, because some data is not available yet
 *   -2  -  invalid element (we should not check this expression more)
 */
static int expr_check_incoming(void);
static int expr_check_outgoing(void);
static int expr_check_protected(void);
static int expr_check_listed(void);
static int expr_check_logic(int e1, int op, int e2);
static int expr_check_phone(const char *phone);
static int expr_check_arop(int num1, int op, int num2);
static int expr_check_flag(const char *str);
static int expr_check_exec(const char *str);
static int expr_check_exist(const char *str);
static int expr_check_port(const char *str);
static int expr_check_mailer(const char *str);
static int expr_check_addr(const char *str);
static int expr_check_time(const char *str);

static int yylex(void);
static int yyparse(void);
static int yyerror(const char *s);

#line 60 "y.tab.c"

#ifndef YYSTYPE
typedef int YYSTYPE;
#endif

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define COMMA 257
#define TEXT 258
#define NUMBER 259
#define INCOMING 260
#define OUTGOING 261
#define LISTED 262
#define PROTECTED 263
#define CONNSPEED 264
#define EXEC 265
#define EXIST 266
#define PORT 267
#define MAILER 268
#define TZ 269
#define FLAG 270
#define SPEED 271
#define PHONE 272
#define TIME 273
#define ADDRESS 274
#define EQ 275
#define NE 276
#define GT 277
#define GE 278
#define LT 279
#define LE 280
#define AND 281
#define OR 282
#define NOT 283
#define XOR 284
#define OPENB 285
#define CLOSEB 286
#define AROP 287
#define LOGOP 288
#define YYERRCODE 256
static const short yylhs[] = {                           -1,
    0,    1,    1,    1,    1,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    3,    3,    4,    4,
};
static const short yylen[] = {                            2,
    1,    1,    2,    3,    3,    1,    1,    1,    1,    2,
    3,    3,    3,    2,    2,    2,    2,    2,    2,    1,
    1,    3,    1,    3,
};
static const short yydefred[] = {                         0,
   20,    6,    7,    8,    9,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    2,
    0,   16,   17,   18,   19,    0,    0,   10,    0,   14,
    0,   15,    0,    0,    0,   11,   13,    0,   12,    0,
    5,    0,   22,   24,
};
static const short yydgoto[] = {                         18,
   19,   20,   28,   32,
};
static const short yysindex[] = {                      -251,
    0,    0,    0,    0,    0, -287, -255, -254, -250, -232,
 -259, -229, -257, -227, -225, -251, -251,    0, -253,    0,
 -223,    0,    0,    0,    0, -222, -219,    0, -220,    0,
 -217,    0, -253, -261, -251,    0,    0, -229,    0, -225,
    0, -253,    0,    0,
};
static const short yyrindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   41,    0,
    0,    0,    0,    0,    0,    0,    1,    0,    0,    0,
    2,    0,    5,    0,    0,    0,    0,    0,    0,    0,
    0,    6,    0,    0,
};
static const short yygindex[] = {                         0,
    7,    0,    8,    3,
};
#define YYTABLESIZE 292
static const short yytable[] = {                         21,
   21,   23,   22,   23,    3,    4,    1,   24,    2,    3,
    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,
   14,   15,   33,   34,   41,   25,   35,   26,   27,   29,
   30,   16,   31,   17,   35,   36,   37,   38,   39,   40,
    1,   42,   44,    0,    0,   43,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   21,   23,   21,   23,
    3,    4,
};
static const short yycheck[] = {                        287,
    0,    0,  258,  258,    0,    0,  258,  258,  260,  261,
  262,  263,  264,  265,  266,  267,  268,  269,  270,  271,
  272,  273,   16,   17,  286,  258,  288,  287,  258,  287,
  258,  283,  258,  285,  288,  259,  259,  257,  259,  257,
    0,   35,   40,   -1,   -1,   38,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  286,  286,  288,  288,
  286,  286,
};
#define YYFINAL 18
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 288
#if YYDEBUG
static const char *yyname[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"COMMA","TEXT","NUMBER",
"INCOMING","OUTGOING","LISTED","PROTECTED","CONNSPEED","EXEC","EXIST","PORT",
"MAILER","TZ","FLAG","SPEED","PHONE","TIME","ADDRESS","EQ","NE","GT","GE","LT",
"LE","AND","OR","NOT","XOR","OPENB","CLOSEB","AROP","LOGOP",
};
static const char *yyrule[] = {
"$accept : fullline",
"fullline : expression",
"expression : elemexp",
"expression : NOT expression",
"expression : expression LOGOP expression",
"expression : OPENB expression CLOSEB",
"elemexp : INCOMING",
"elemexp : OUTGOING",
"elemexp : LISTED",
"elemexp : PROTECTED",
"elemexp : FLAG flagstring",
"elemexp : CONNSPEED AROP NUMBER",
"elemexp : SPEED AROP NUMBER",
"elemexp : TZ AROP NUMBER",
"elemexp : PHONE TEXT",
"elemexp : TIME timestring",
"elemexp : EXEC TEXT",
"elemexp : EXIST TEXT",
"elemexp : PORT TEXT",
"elemexp : MAILER TEXT",
"elemexp : TEXT",
"flagstring : TEXT",
"flagstring : TEXT COMMA flagstring",
"timestring : TEXT",
"timestring : TEXT COMMA timestring",

};
#endif

int      yydebug;
int      yynerrs;

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH  500
#endif
#endif

#define YYINITSTACKSIZE 500

typedef struct {
    unsigned stacksize;
    short    *s_base;
    short    *s_mark;
    short    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 211 "bforce/expression.y"

#include "expression_lex.c"

static int expr_check_incoming(void)
{
	if( !state.valid )
		return -1;
	
	return state.caller ? 0 : 1;
}

static int expr_check_outgoing(void)
{
	if( !state.valid )
		return -1;
	
	return state.caller ? 1 : 0;
}

static int expr_check_protected(void)
{
	if( !state.valid || !state.node.addr.zone )
		return -1;
	
	return state.protected ? 1 : 0;
}

static int expr_check_listed(void)
{
	if( !state.valid || !state.node.addr.zone )
		return -1;
	
	return state.node.listed ? 1 : 0;
}

static int expr_check_logic(int e1, int op, int e2)
{
	DEB((D_EVENT, "[yacc] logic: %d %d %d", e1, op, e2));
	
	if( e1 < 0 ) e1 = 0;
	if( e2 < 0 ) e2 = 0;
	
	switch(op) {
	case AND:
		return (e1 && e2);
	case OR:
		return (e1 || e2);
	case XOR:
		return (e1  ^ e2);
	}
	
	log("invalid logical operator in expression");
	
	return 0;
}

static int expr_check_phone(const char *phone)
{
	DEB((D_EVENT, "[yacc] phone: is %s in %s",
		phone, state.node.phone));
	
	if( !state.valid )
		return 0;
	
	return strstr(state.node.phone, phone) ? 1 : 0;
}

static int expr_check_arop(int num1, int op, int num2)
{
	DEB((D_EVENT, "[yacc] arop: %d %d %d", num1, op, num2));
	
	switch(op) {
	case EQ: return (num1 == num2);
	case NE: return (num1 != num2);
	case GT: return (num1 >  num2);
	case GE: return (num1 >= num2);
	case LT: return (num1 <  num2);
	case LE: return (num1 <= num2);
	}
	
	log("invalid arithmetic operator in expression");
	
	return 0;
}

static int expr_check_flag(const char *str)
{
	DEB((D_EVENT, "[yacc] flag: \"%s\"", str));
	
	if( !state.valid )
		return 0;
	
	return !nodelist_checkflag(state.node.flags, str);
}

static int expr_check_exec(const char *str)
{
	DEB((D_EVENT, "[yacc] exec: \"%s\"", str));
	
	return session_run_command(str) ? 0 : 1;
}

static int expr_check_exist(const char *str)
{
	DEB((D_EVENT, "[yacc] exist: \"%s\"", str));
	
	return access(str, F_OK) ? 1 : 0;
}

static int expr_check_port(const char *str)
{
	DEB((D_EVENT, "[yacc] port: \"%s\"", str));
	
	if( !state.valid )
		return -1;
	
	if( !strcasecmp(str, "tcpip") )
	{
		return state.inet ? 1 : 0;
	}
	else if( state.modemport && state.modemport->name )
	{
		return strstr(state.modemport->name, str) ? 1 : 0;
	}
	else if( isatty(0) )
	{
		return strstr(ttyname(0), str) ? 1 : 0;
	}
	
	return -1;
}

static int expr_check_mailer(const char *str)
{
	char *p;
	
	DEB((D_EVENT, "[yacc] mailer: \"%s\"", str));
	
	if( !state.valid )
		return -1;
	
	if( state.handshake && state.handshake->remote_mailer )
		p = state.handshake->remote_mailer(state.handshake);
	else
		return -1;
	
	return (p && string_casestr(p, str)) ? 1 : 0;
}

static int expr_check_addr(const char *str)
{
	s_faddr addr;
#ifdef DEBUG
	char abuf1[BF_MAXADDRSTR+1];
	char abuf2[BF_MAXADDRSTR+1];
	
	DEB((D_EVENT, "[yacc] addr: \"%s\" (session with \"%s\")",
		ftn_addrstr(abuf1, addr),
		ftn_addrstr(abuf2, state.node.addr)));
#endif
	
	if( !state.valid )
		return -1;

	if( ftn_addrparse(&addr, str, TRUE) )
	{
		log("invalid address \"%s\"", str);
		return -2;
	}
	
	return !ftn_addrcomp_mask(state.node.addr, addr);
}

static int expr_check_time(const char *str)
{
	DEB((D_EVENT, "[yacc] time: \"%s\"", str));
	
	switch( time_check(str, now) ) {
	case -1:
		log("invalid time string \"%s\"", str);
		return -2;
	case  0:
		return 1;
	}

	return 0;
}

bool eventexpr(s_expr *expr)
{
	time_t tt;
	char *tmp;

	if( !expr || !expr->expr )
		return 1;

	if( expr->error )
		return 0;
	
	DEB((D_EVENT, "eventexpr: [yacc] check expression \"%s\"",
		expr->expr));
	
	tt  = time(NULL);
	now = localtime(&tt);
	tmp = (char*)xstrcpy(expr->expr);

	expr_p_pos = tmp;
	expr_result = 0;
	
	if( yyparse() )
	{
		expr->error = TRUE;
		
		log("cannot parse expression \"%s\"", expr->expr);
		DEB((D_EVENT, "eventexpr: [yacc] $yyparse return error"));
		
		expr_result = 0;
	}
	
	free(tmp);
	
	DEB((D_EVENT, "eventexpr: [yacc] checking result is \"%s\" (%d)",
		(expr_result == 1) ? "TRUE" : "FALSE", expr_result));
	
	return (expr_result == 1) ? TRUE : FALSE;
}

static int yyerror(const char *str)
{
	log("expression check failure: %s", str);
	
	return 0;
}
#line 548 "y.tab.c"

#if YYDEBUG
#include <stdio.h>		/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = data->s_mark - data->s_base;
    newss = (short *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack)) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = YYLEX) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
        {
            goto yyoverflow;
        }
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;

    yyerror("syntax error");

    goto yyerrlab;

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystack.s_mark]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
                {
                    goto yyoverflow;
                }
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 1:
#line 93 "bforce/expression.y"
	{
		DEB((D_EVENT, "[yacc] expression return %d", yystack.l_mark[0]));
		expr_result = yystack.l_mark[0];
	}
break;
case 2:
#line 99 "bforce/expression.y"
	{
		DEB((D_EVENT, "[yacc] elemexp return %d", yystack.l_mark[0]));
		yyval = yystack.l_mark[0];
	}
break;
case 3:
#line 104 "bforce/expression.y"
	{
		DEB((D_EVENT, "[yacc] not exprression %d", yystack.l_mark[0]));
		if( (yystack.l_mark[0]) < 0 )
			yyval = 0;
		else
			yyval = !(yystack.l_mark[0]);
	}
break;
case 4:
#line 112 "bforce/expression.y"
	{
		yyval = expr_check_logic(yystack.l_mark[-2], yystack.l_mark[-1], yystack.l_mark[0]);
	}
break;
case 5:
#line 116 "bforce/expression.y"
	{
		DEB((D_EVENT, "eventexp: [yacc] backeted.expr %d", yystack.l_mark[-1]));
		yyval = yystack.l_mark[-1];
	}
break;
case 6:
#line 122 "bforce/expression.y"
	{
		yyval = expr_check_incoming();
	}
break;
case 7:
#line 126 "bforce/expression.y"
	{
		yyval = expr_check_outgoing();
	}
break;
case 8:
#line 130 "bforce/expression.y"
	{
		yyval = expr_check_listed();
	}
break;
case 9:
#line 134 "bforce/expression.y"
	{
		yyval = expr_check_protected();
	}
break;
case 10:
#line 138 "bforce/expression.y"
	{
		yyval = yystack.l_mark[0];
	}
break;
case 11:
#line 142 "bforce/expression.y"
	{
		if( state.valid && state.connspeed > 0 )
			yyval = expr_check_arop(state.connspeed, yystack.l_mark[-1], yystack.l_mark[0]);
		else
			yyval = -1;
	}
break;
case 12:
#line 149 "bforce/expression.y"
	{
		if( state.valid && state.node.speed > 0 )
			yyval = expr_check_arop(state.node.speed, yystack.l_mark[-1], yystack.l_mark[0]);
		else
			yyval = -1;
	}
break;
case 13:
#line 156 "bforce/expression.y"
	{
		yyval = expr_check_arop(time_gmtoffset(), yystack.l_mark[-1], yystack.l_mark[0]);
	}
break;
case 14:
#line 160 "bforce/expression.y"
	{
		yyval = expr_check_phone(expr_p_text);
	}
break;
case 15:
#line 164 "bforce/expression.y"
	{
		yyval = yystack.l_mark[0];
	}
break;
case 16:
#line 168 "bforce/expression.y"
	{
		yyval = expr_check_exec(expr_p_text);
	}
break;
case 17:
#line 172 "bforce/expression.y"
	{
		yyval = expr_check_exist(expr_p_text);
	}
break;
case 18:
#line 176 "bforce/expression.y"
	{
		yyval = expr_check_port(expr_p_text);
	}
break;
case 19:
#line 180 "bforce/expression.y"
	{
		yyval = expr_check_mailer(expr_p_text);
	}
break;
case 20:
#line 184 "bforce/expression.y"
	{
		yyval = expr_check_addr(expr_p_text);
		if( yyval == -2 )
			YYABORT;
	}
break;
case 21:
#line 191 "bforce/expression.y"
	{
		yyval = expr_check_flag(expr_p_text);
	}
break;
case 22:
#line 195 "bforce/expression.y"
	{
		yyval = expr_check_logic(yystack.l_mark[-2], OR, yystack.l_mark[0]);
	}
break;
case 23:
#line 200 "bforce/expression.y"
	{
		yyval = expr_check_time(expr_p_text);
		if( yyval == -2 )
			YYABORT;
	}
break;
case 24:
#line 206 "bforce/expression.y"
	{
		yyval = expr_check_logic(yystack.l_mark[-2], OR, yystack.l_mark[0]);
	}
break;
#line 915 "y.tab.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            if ((yychar = YYLEX) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
    {
        goto yyoverflow;
    }
    *++yystack.s_mark = (short) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
