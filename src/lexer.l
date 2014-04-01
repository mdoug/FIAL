%option noyywrap never-interactive nounistd

%{
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include "ast.h"
#include "basic_types.h"
#include "interp.h"

#include "parser_defines.h"

#include "parser.tab.h"

#define PARSE_ERROR(X)    assert((0, X))
#define PARSE_WARNING(X)  assert((0, X))



#define YY_USER_ACTION yylloc.line   = FIAL_parser_line_no; yylloc.col = FIAL_parser_col_no;\
		       FIAL_parser_col_no += yyleng;

extern struct FIAL_interpreter *current_interp;

    /*not sure why i'm bothering doing it this way, but this might
    ease the transition to a reentrant parser somehow. */

extern int FIAL_parser_line_no;
extern int FIAL_parser_col_no;

struct text_buffer {
	char *buf;
	int cap;
	int len;
};

static void append_str  (struct text_buffer *, char *str, int len);
static void append_char (struct text_buffer *, char);
%}

%x comment string_state

DIGIT [0-9]

%%

	struct text_buffer buffy;

"if"          {return IF;}
"var"         {return VAR;}
    /* ok, I am testing out "leave" and "redo" for "break" and "continue" */
"leave"       {return LEAVE;}
"redo"        {return REDO;}
"call"        {return CALL;}
"and"         {return AND;}
"or"          {return OR;}
"not"         {return NOT;}

{DIGIT}+             {
			errno = 0;
		        long tmp = strtol(yytext, NULL, 10);
			if(tmp == LONG_MAX || tmp == LONG_MIN) {
			       if (errno == ERANGE) {
PARSE_WARNING("parsed value out of range!");
			       }
			} else if(tmp > INT_MAX || tmp < INT_MIN) {
			       PARSE_WARNING("parsed value out of range!");
			}
			yylval.n = (int) tmp;
			return INT;
		      }

{DIGIT}*"."{DIGIT}+  {
		        errno = 0;
			double tmp = strtod(yytext, NULL);
			if(tmp == HUGE_VAL || tmp == -HUGE_VAL) {
				if (errno == ERANGE) {
					PARSE_WARNING("parsed value out of range!");
				}
			} else if(tmp > FLT_MAX || tmp < FLT_MIN) {
			       PARSE_WARNING("parsed value out of range!");
			}
			yylval.x =(float) tmp;
			return FLOAT;
		      }

[a-zA-Z_][a-zA-Z_0-9]* {
		        FIAL_get_symbol(&yylval.sym, yytext, current_interp);
			return ID;
			}
$[a-zA-Z_][a-zA-Z_0-9]* {
			FIAL_get_symbol(&yylval.sym,yytext + 1,current_interp);
			return SYMBOL;
			}

"/*"         BEGIN(comment);

<comment>[^*\n]*        /* eat anything that's not a '*' */
<comment>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<comment>\n             {FIAL_parser_line_no++; FIAL_parser_col_no = 1;}
<comment>"*"+"/"        BEGIN(INITIAL);


\"                     {memset(&buffy, 0, sizeof(buffy));
		        BEGIN(string_state);}

<string_state>\"               {
			if(!buffy.buf) {
			      yylval.str = malloc(1);
			      yylval.str[0] = '\0';
		        }
			else {
			     yylval.str = buffy.buf;
		        }
			BEGIN(INITIAL);
			return STRING;
                      }
<string_state>\n       {FIAL_parser_line_no++; FIAL_parser_col_no = 1;
		       append_char(&buffy, '\n');}

<string_state>\\n               append_char(&buffy, '\n');
<string_state>\\t               append_char(&buffy, '\t');
<string_state>\\r               append_char(&buffy, '\r');
<string_state>\\b               append_char(&buffy, '\b');
<string_state>\\f               append_char(&buffy, '\f');

<string_state>\\\n            {FIAL_parser_line_no++; FIAL_parser_col_no = 1;
		                 append_char(&buffy, '\n');}

<string_state>\\.             append_char(&buffy, yytext[1]);
<string_state>[^\\\n\"]+        {
		           append_str(&buffy, yytext, yyleng);
		       }

 "{"                 {return '{';}
 "}"                 {return '}';}
 ","                 {return ',';}
 "("                 {return '(';}
 ")"                 {return ')';}
 ";"                 {return ';';}
 "+"                 {return '+';}
 "-"                 {return '-';}
 "*"                 {return '*';}
 "/"                 {return '/';}
 "="                 {return '=';}
 "=="                {return EQUALITY;}
 "<"                 {return '<';}
 ">"                 {return '>';}
 ":"                 {return ':';}
 "."                 {return '.';}

 [[:blank:]]         {}
 \n                  {FIAL_parser_line_no++;
 		     FIAL_parser_col_no = 1;}

%%

static YY_BUFFER_STATE buffer_state = NULL;

void setup_new_input_buffer (FILE *file)
{
	yy_delete_buffer(buffer_state);
	buffer_state = yy_create_buffer(file, YY_BUF_SIZE);
	yy_switch_to_buffer(buffer_state);
}

#define MIN_BUFFER_SIZE 16

/*FIXME allocation errors... */

static void create_buf  (struct text_buffer  *buffy, int len)
{
	if(!buffy->buf) {
		int new_size = (len + 1 > MIN_BUFFER_SIZE) ? len + 1 :
		    	       MIN_BUFFER_SIZE;
		buffy->buf = malloc(new_size);
		memset(buffy->buf, 0, new_size);
		buffy->cap = new_size;
	}
}

static void reserve_buf (struct text_buffer  *buffy, int len)
{
	if((buffy->cap - buffy->len - 1) < len) {
	        int new_size = buffy->cap > len + 1 ?
		    	       buffy->cap * 2 : buffy->cap + len + 1;
		char *tmp = realloc(buffy->buf, new_size);
		assert(tmp);  /* ugh this is aweful. FIXME */
		buffy->buf = tmp;
		memset(buffy->buf + buffy->cap, 0, new_size - buffy->cap);
		buffy->cap = new_size;
	}
}

static void append_str (struct text_buffer *buffy, char *str, int len)
{
	if(!buffy->buf) {
			create_buf(buffy, len);
	}
	assert(buffy->buf);
	reserve_buf(buffy, len);
	memmove(buffy->buf + buffy->len, str, len);
	buffy->len += len;
}

static void append_char (struct text_buffer *buffy, char c)
{
	if(buffy->buf)
		create_buf(buffy, 1);
	assert(buffy->len <= buffy->cap);
	reserve_buf(buffy, 1);
	buffy->buf[buffy->len++] = c;

}
