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

%token COMMA
%token TEXT
%token NUMBER
%token INCOMING
%token OUTGOING
%token LISTED
%token PROTECTED
%token CONNSPEED
%token EXEC
%token EXIST
%token PORT
%token MAILER
%token TZ
%token FLAG
%token SPEED
%token PHONE
%token TIME
%token ADDRESS
%token EQ
%token NE
%token GT
%token GE
%token LT
%token LE
%token AND
%token OR
%token NOT
%token XOR
%token OPENB
%token CLOSEB
%token AROP
%token LOGOP

%expect 2
%{
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

%}

%%
fullline : expression
	{
		DEB((D_EVENT, "[yacc] expression return %d", $1));
		expr_result = $1;
	}
	;
expression : elemexp
	{
		DEB((D_EVENT, "[yacc] elemexp return %d", $1));
		$$ = $1;
	}
| NOT expression
	{
		DEB((D_EVENT, "[yacc] not exprression %d", $2));
		if( ($2) < 0 )
			$$ = 0;
		else
			$$ = !($2);
	}
| expression LOGOP expression
	{
		$$ = expr_check_logic($1, $2, $3);
	}
| OPENB expression CLOSEB
	{
		DEB((D_EVENT, "eventexp: [yacc] backeted.expr %d", $2));
		$$ = $2;
	}
	;
elemexp	: INCOMING
	{
		$$ = expr_check_incoming();
	}
| OUTGOING
	{
		$$ = expr_check_outgoing();
	}
| LISTED
	{
		$$ = expr_check_listed();
	}
| PROTECTED
	{
		$$ = expr_check_protected();
	}
| FLAG flagstring
	{
		$$ = $2;
	}
| CONNSPEED AROP NUMBER
	{
		if( state.valid && state.connspeed > 0 )
			$$ = expr_check_arop(state.connspeed, $2, $3);
		else
			$$ = -1;
	}
| SPEED AROP NUMBER
	{
		if( state.valid && state.node.speed > 0 )
			$$ = expr_check_arop(state.node.speed, $2, $3);
		else
			$$ = -1;
	}
| TZ AROP NUMBER
	{
		$$ = expr_check_arop(time_gmtoffset(), $2, $3);
	}
| PHONE TEXT
	{
		$$ = expr_check_phone(expr_p_text);
	}
| TIME timestring
	{
		$$ = $2;
	}
| EXEC TEXT
	{
		$$ = expr_check_exec(expr_p_text);
	}
| EXIST TEXT
	{
		$$ = expr_check_exist(expr_p_text);
	}
| PORT TEXT
	{
		$$ = expr_check_port(expr_p_text);
	}
| MAILER TEXT
	{
		$$ = expr_check_mailer(expr_p_text);
	}
| TEXT
	{
		$$ = expr_check_addr(expr_p_text);
		if( $$ == -2 )
			YYABORT;
	}
	;
flagstring : TEXT
	{
		$$ = expr_check_flag(expr_p_text);
	}
| TEXT COMMA flagstring
	{
		$$ = expr_check_logic($1, OR, $3);
	}
	;
timestring : TEXT
	{
		$$ = expr_check_time(expr_p_text);
		if( $$ == -2 )
			YYABORT;
	}
| TEXT COMMA timestring
	{
		$$ = expr_check_logic($1, OR, $3);
	}
	;
%%

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
