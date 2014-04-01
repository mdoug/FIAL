%{
    #include <stdio.h>
    #include <string.h>
    #include "interp.h"
    #include "ast.h"
    #include "ast_def_short.h"
    #include "error_def_short.h"
    #include "basic_types.h"
    #include "parser_defines.h"

    typedef struct FIAL_ast_node node;
    typedef int symbol;

    /*this will get cleaned up, for now a global variable works ok.
      Don't want to mess with bison settings right now.  */
    struct FIAL_interpreter *current_interp;

    #define get_ast_node(x, y, z) FIAL_get_ast_node(x, y, z)
    #define NODE(x, y, z, L, C) FIAL_get_ast_node_with_loc (x, y, z, L, C);

    int count = 0;
    int yylex(void);
    void yyerror(char const *);

    extern int yylineno;

    node *top = NULL;
    int FIAL_parser_line_no = 1;
    int FIAL_parser_col_no  = 1;

    struct FIAL_error_info *FIAL_parser_error_info;

%}

%initial-action {@$.line = @$.col = 1;}

/*  for now I am using floats.  On 64 bit machines, doubles will make
    more sense, but floats would work there as well. */

%union {
    int n;
    FIAL_symbol sym;
    float x;
    struct FIAL_ast_node *ast_node;
    char *str;
}

%token <n> INT
%token <x> FLOAT
%token <sym> ID SYMBOL
%token <str> STRING

%token VAR CALL IF LEAVE REDO AND NOT OR

%type <ast_node> expr assign statement statements body block redo if leave
%type <ast_node> arglist proc_def var_decl call top labelled_body common_body

%left '+' '-'
%left '*' '/'
%left NEG
%left '<' '>' EQUALITY
%left AND OR NOT /* not sure where these should go, or what there
		    precedence should be in relation to one another,
		    but this seems more or less right. */
%%

very_top:
top              {top = $1;}

top:
proc_def         {node *tmp = NODE(AST_TOP_NODE, $1, NULL, @$.line, @$.col);
		  $$ = NODE(AST_TOP, tmp, tmp, @$.line, @$.col); }
| top proc_def   {$$ = $1;
                  $$->right->right = NODE(AST_TOP_NODE, $2, NULL, @$.line, @$.col);
	          $$->right = $$->right->right;}
;

proc_def:
ID body                   {$$ = NODE(AST_PROC_DEF, NULL, $2, @$.line, @$.col);  $$->sym = $1;}
| ID '(' arglist ')' body {$$ = NODE(AST_PROC_DEF, $3, $5, @$.line, @$.col);  $$->sym = $1;}
;

/* this stores the last item in the list in the first items ->left field. */

arglist:
/*EMPTY*/               {$$ = NULL;}
| ID                    {$$ = NODE(AST_ARGLIST, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| arglist ',' ID        {node *tmp = NODE(AST_ARGLIST, NULL, NULL, @$.line, @$.col); $$ = $1;
                         tmp->sym = $3;
               		 if(!$$->left) {
     			 	$$->right = $$->left = tmp;
			 } else {
				$$->left->right = tmp;
				$$->left = $$->left->right;
			 }
			 }
;

/*
 * Ok, I have to add back in smoe stuff.  I took this out of statements:

| break                {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| continue             {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}

  Because I have to change it to only allow leave and redo as the last
  statement in a body.

 */



body:
'{'  common_body      {$$ = $2;}
;

/*
   Unfortunately, this has to be separate from "body", because proc
   bodies can't have labels.

   This is a little bit of a cheat, since statemsnts doesn't use a
   value, I am just sticking the label on it, and then I can pass this
   up to block.  This stuff could be reworked a little bit, and I
   probably should fix up my trees when I rewrite the interpreter to
   use better internal data structures.

 */

labelled_body:
'{' ID ':'   common_body   {$$ = $4; $$->sym = $2;}
;

/* common between body and labelled body.  Have to manually add leave
and redo statements, since they end a block.  */

common_body:
statements '}'      {$$ = $1;}
| statements leave  {node *tmp = NODE(AST_STMT, $2, NULL, @2.line, @2.col);
                     $$ = $1; $$->right->right = tmp; $$->right = tmp;}
| statements redo   {node *tmp = NODE(AST_STMT, $2, NULL, @2.line, @2.col);
                     $$ = $1; $$->right->right = tmp; $$->right = tmp;}

| '}'               {$$ = NODE(AST_STMTS, NULL, NULL, @$.line, @$.col);}
| leave             {node *tmp = NODE(AST_STMT, $1, NULL, @$.line, @$.col);
                     $$ = NODE(AST_STMTS, tmp, tmp, @$.line, @$.col);}
| redo              {node *tmp = NODE(AST_STMT, $1, NULL, @$.line, @$.col);
                     $$ = NODE(AST_STMTS, tmp, tmp, @$.line, @$.col);}

statements:
statement              {$$ = NODE(AST_STMTS, $1, $1, @$.line, @$.col);}
| statements statement {$$ = $1;  $$->right->right = $2;  $$->right = $2;}
;

statement:
var_decl               {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| call                 {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| assign               {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| if                   {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| block                {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
;

var_decl:
VAR ID                 {$$ = NODE(AST_VAR_DECL, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| var_decl ',' ID      {$$ = NODE(AST_VAR_DECL, NULL, NULL, @$.line, @$.col); $$->sym = $3;
	                $$->right = $1;}  /*reverses order, should be ok */
;

   /* for a while I was using commas instead of periods, but when I
      eliminated the semicolons at the end of lines, it began to look
      strange.  So I am just changing it now, the comma syntax is just
      worse.  I will at least try this for a bit. */

call:
ID                    '(' arglist ')'   {$$ = NODE(AST_CALL_A, $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID                                    {$$ = NODE(AST_CALL_A, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| ID '.' ID           '(' arglist ')'   {node *tmp = NODE(AST_ID, NULL, NULL, @$.line, @$.col);  tmp->sym = $3;
				         $$ = NODE(AST_CALL_B, tmp, $5, @$.line, @$.col); $$->sym = $1;}
| ID '.' ID                             {node *tmp = NODE(AST_ID, NULL, NULL, @$.line, @$.col);  tmp->sym = $3;
				         $$ = NODE(AST_CALL_B, tmp, NULL, @$.line, @$.col); $$->sym = $1;}

/*
    This is an idea -- it was working at one time, but I don't see the
    need for it just now.  I might put it back though. It would allow
    you to to things like:

    var ohmygosh;
    set_ohmyghosh(ohmygosh);
    call $stupid_lib, ohmygosh;

    But -- I am not sold on this idea.  I like the idea of getting
    callbacks from a library function, e.g.. --

    var my_callback, label, proc;
    label = "lib label";
    proc  = $some_proc;
    omni, get_callback (my_callback, label , proc);

    Which strikes me as a good general mechanism, that can work across
    the C interface.  (The idea is that the generated callback will be
    callable by C with a different execution environment.  But, this
    mechanism would still be useful in FIAL).

 */

/*
| CALL expr           '(' arglist ')'   {$$ = NODE(AST_CALL_C, $2, $4, @$.line, @$.col);}
| CALL expr ',' expr  '(' arglist ')'   {node *tmp = NODE(AST_EXPR_PAIR, $2, $4, @$.line, @$.col);
	                                 $$ = NODE(AST_CALL_D, tmp, $6, @$.line, @$.col);}*/
;

assign:
ID '=' expr   {$$ = NODE(AST_ASSIGN, $3, NULL, @$.line, @$.col); $$->sym = $1;}
;

if:
IF expr block {$$ = NODE(AST_IF, $2, $3, @$.line, @$.col);}
;

/* ok, not sure how to do this, since now I am moving the label into
   the body.  I guess blocks will just be different things from proc
   bodies, that's fine. */

block:
body              {$$ = NODE(AST_BLOCK, $1, NULL, @$.line, @$.col);}
| labelled_body   {$$ = NODE(AST_BLOCK, $1, NULL, @$.line, @$.col); $$->sym = $1->sym;}
;

leave:
LEAVE ':' ID '}'   {$$ = NODE(AST_BREAK, NULL, NULL, @$.line, @$.col); $$->sym = $3;}
| LEAVE ID '}'     {$$ = NODE(AST_BREAK, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
;

redo:
REDO '}'          {$$ = NODE(AST_CONTINUE, NULL, NULL, @$.line, @$.col);}
| REDO ':' ID '}' {$$ = NODE(AST_CONTINUE, NULL, NULL, @$.line, @$.col); $$->sym = $3;}
| REDO ID '}'     {$$ = NODE(AST_CONTINUE, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
;

expr:
INT                     {$$ = NODE(AST_INT,     NULL, NULL, @$.line, @$.col); $$->n   = $1;}
| FLOAT                 {$$ = NODE(AST_FLOAT,   NULL, NULL, @$.line, @$.col); $$->x   = $1;}
| ID                    {$$ = NODE(AST_EXPR_ID, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| SYMBOL                {$$ = NODE(AST_SYMBOL,  NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| STRING                {$$ = NODE(AST_STRING,  NULL, NULL, @$.line, @$.col); $$->str = $1;}

| '-' expr %prec NEG    {$$ = NODE(AST_NEG,          $2, NULL, @$.line, @$.col);}
| '(' expr ')'          {$$ = $2; }
| expr '+' expr         {$$ = NODE(AST_PLUS,         $1,   $3, @$.line, @$.col);}
| expr '-' expr         {$$ = NODE(AST_MINUS,        $1,   $3, @$.line, @$.col);}
| expr '*' expr         {$$ = NODE(AST_TIMES,        $1,   $3, @$.line, @$.col);}
| expr '/' expr         {$$ = NODE(AST_DIVIDE,       $1,   $3, @$.line, @$.col);}
| expr EQUALITY expr    {$$ = NODE(AST_EQUALITY,     $1,   $3, @$.line, @$.col);}
| expr '<' expr         {$$ = NODE(AST_LESS_THAN,    $1,   $3, @$.line, @$.col);}
| expr '>' expr         {$$ = NODE(AST_GREATER_THAN, $1,   $3, @$.line, @$.col);}
| expr AND expr         {$$ = NODE(AST_AND,          $1,   $3, @$.line, @$.col);}
| expr OR  expr         {$$ = NODE(AST_OR,           $1,   $3, @$.line, @$.col);}
| NOT expr              {$$ = NODE(AST_NOT,          $2, NULL, @$.line, @$.col);}
;

%%

void yyerror (char const *err_msg)
{
	FIAL_parser_error_info->code = ERROR_PARSE;
	FIAL_parser_error_info->line = yylloc.line;
	FIAL_parser_error_info->col  = yylloc.col;
	FIAL_parser_error_info->static_msg = err_msg;
}
