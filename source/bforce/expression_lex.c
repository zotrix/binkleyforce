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

typedef struct lextable {
const	char *str;
	int value;
	int retc;
	int len;
} s_lextable;

static bool initialised = FALSE;
static s_lextable *next = NULL;

static s_lextable operators[] =
{
	{ "==",          EQ,          AROP,        0 },
	{ "!=",          NE,          AROP,        0 },
	{ ">",           GT,          AROP,        0 },
	{ ">=",          GE,          AROP,        0 },
	{ "<",           LT,          AROP,        0 },
	{ "<=",          LE,          AROP,        0 },
	{ "(",           OPENB,       OPENB,       0 },
	{ ")",           CLOSEB,      CLOSEB,      0 },
	{ "&&",          AND,         LOGOP,       0 },
	{ "&",           AND,         LOGOP,       0 },
	{ "||",          OR,          LOGOP,       0 },
	{ "|",           OR,          LOGOP,       0 },
	{ "Xor",         XOR,         LOGOP,       0 },
	{ "!",           NOT,         NOT,         0 },
	{ ",",           COMMA,       COMMA,       0 },
	{ NULL,          0,           0,           0 }
};

s_lextable elements[] =
{
	{ "TZ",          TZ,          TZ,          0 },
	{ "Address",     ADDRESS,     ADDRESS,     0 },
	{ "Time",        TIME,        TIME,        0 },
	{ "Phone",       PHONE,       PHONE,       0 },
	{ "Flag",        FLAG,        FLAG,        0 },
	{ "Incoming",    INCOMING,    INCOMING,    0 },
	{ "Outgoing",    OUTGOING,    OUTGOING,    0 },
	{ "Listed",      LISTED,      LISTED,      0 },
	{ "Protected",   PROTECTED,   PROTECTED,   0 },
	{ "Connspeed",   CONNSPEED,   CONNSPEED,   0 },
	{ "Exec",        EXEC,        EXEC,        0 },
	{ "Exist",       EXIST,       EXIST,       0 },
	{ "Port",        PORT,        PORT,        0 },
	{ "Mailer",      MAILER,      MAILER,      0 },
	{ NULL,          0,           0,           0 }
};

static void expr_inittables(void)
{
	int i;
	
	for( i = 0; operators[i].str; i++ )
		operators[i].len = strlen(operators[i].str);

	for( i = 0; elements[i].str; i++ )
		elements[i].len = strlen(elements[i].str);
}

static int yylex(void)
{
	int i, retc = 0;
	s_lextable *entry = NULL;

	ASSERT(expr_p_pos != NULL);
	
	if( !initialised )
		{ expr_inittables(); initialised = TRUE; }

	if( next )
	{
		/*
		 *  Found operator left from previous call to yylex()
		 */
		yylval = next->value;
		DEB((D_EVENT, "yylex: got next \"%s\", yylval = %d, ret = %d",
			next->str, next->value, next->retc));
		retc = next->retc;
		next = NULL;
		return retc;
	}
			
	while( isspace(*expr_p_pos) ) expr_p_pos++;

	if( *expr_p_pos == '\0' ) return 0;
		
	for( i = 0; operators[i].str; i++ )
	{
		if( strncasecmp(expr_p_pos, operators[i].str, operators[i].len) == 0 )
			{ entry = &operators[i]; break; }
	}
	
	if( entry == NULL )
	{
		for( i = 0; elements[i].str; i++ )
		{
			if( strncasecmp(expr_p_pos, elements[i].str, elements[i].len) == 0 )
				{ entry = &elements[i]; break; }
		}
	}

	if( entry )
	{
		expr_p_pos += entry->len;
		yylval = entry->value;
		DEB((D_EVENT, "yylex: got token \"%s\", yylval = %d, ret = %d",
			entry->str, entry->value, entry->retc));
		return entry->retc;
	}
	else
	{

		if( *expr_p_pos == '"' )
		{
			expr_p_text = ++expr_p_pos;
			
			while( *expr_p_pos != '"' && *expr_p_pos )
				++expr_p_pos;
		}
		else
		{
			expr_p_text = expr_p_pos++;
			
			while( !isspace(*expr_p_pos) && *expr_p_pos )
			{
				for( i = 0; operators[i].str; i++ )
					if( strncasecmp(expr_p_pos, operators[i].str, operators[i].len) == 0 )
					{
						next = &operators[i];
						goto enough;
					}
				expr_p_pos++;
			}
		}

enough:	
		if( *expr_p_pos )
		{
			*expr_p_pos = '\0';			
			if( next && next->len > 1 )
				expr_p_pos += next->len;
			else
				expr_p_pos += 1;
		}
		
		if( ISDEC(expr_p_text) )
		{
			/* It is decimal number */
			yylval = atol(expr_p_text);
			DEB((D_EVENT, "yylex: got NUMBER %d", yylval));
			return NUMBER;
		}
		else
		{
			/* It is text */
			DEB((D_EVENT, "yylex: got TEXT \"%s\"", expr_p_text));
			return(yylval = TEXT);
		}
	}
}
