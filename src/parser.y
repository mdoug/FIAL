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

%token VAR CALL IF BREAK CONTINUE

%type <ast_node> expr assign statement statements body block if break continue
%type <ast_node> arglist proc_def var_decl call top

%left '+' '-'
%left '*' '/'
%left NEG
%left '<' '>' EQUALITY

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

body:
'{' statements '}'      {$$ = $2;}
| '{' '}'                 {$$ = NODE(AST_STMTS, NULL, NULL, @$.line, @$.col);}
;

statements:
statement              {$$ = NODE(AST_STMTS, $1, $1, @$.line, @$.col);}
| statements statement {$$ = $1;  $$->right->right = $2;  $$->right = $2;}
;

statement:
var_decl   ';'         {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| call     ';'         {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| assign   ';'         {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| if                   {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| block                {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| break    ';'         {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
| continue ';'         {$$ = NODE(AST_STMT, $1, NULL, @$.line, @$.col);}
;

var_decl:
VAR ID                 {$$ = NODE(AST_VAR_DECL, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| var_decl ',' ID      {$$ = NODE(AST_VAR_DECL, NULL, NULL, @$.line, @$.col); $$->sym = $3;
	                $$->right = $1;}  /*reverses order, should be ok */
;

call:
ID                    '(' arglist ')'   {$$ = NODE(AST_CALL_A, $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID                                    {$$ = NODE(AST_CALL_A, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| ID ',' ID           '(' arglist ')'   {node *tmp = NODE(AST_ID, NULL, NULL, @$.line, @$.col);  tmp->sym = $3;
				         $$ = NODE(AST_CALL_B, tmp, $5, @$.line, @$.col); $$->sym = $1;}
| ID ',' ID                             {node *tmp = NODE(AST_ID, NULL, NULL, @$.line, @$.col);  tmp->sym = $3;
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

block:
body          {$$ = NODE(AST_BLOCK, $1, NULL, @$.line, @$.col);}
| ID body     {$$ = NODE(AST_BLOCK, $2, NULL, @$.line, @$.col); $$->sym = $1;}
;

break:
BREAK         {$$ = NODE(AST_BREAK, NULL, NULL, @$.line, @$.col);}
| BREAK ID    {$$ = NODE(AST_BREAK, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
;

continue:
CONTINUE         {$$ = NODE(AST_CONTINUE, NULL, NULL, @$.line, @$.col);}
| CONTINUE ID    {$$ = NODE(AST_CONTINUE, NULL, NULL, @$.line, @$.col); $$->sym = $2;}
;

expr:
INT                     {$$ = NODE(AST_INT,     NULL, NULL, @$.line, @$.col); $$->n   = $1;}
| FLOAT                 {$$ = NODE(AST_FLOAT,   NULL, NULL, @$.line, @$.col); $$->x   = $1;}
| ID                    {$$ = NODE(AST_EXPR_ID, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| SYMBOL                {$$ = NODE(AST_SYMBOL,  NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| STRING                {$$ = NODE(AST_STRING,  NULL, NULL, @$.line, @$.col); $$->str = $1;}

| '-' expr %prec NEG    {$$ = NODE(AST_NEG,          $2, NULL, @$.line, @$.col);}
| '(' expr ')'          {$$ = $2; }
| expr '+' expr         {$$ = NODE(AST_PLUS,         $1, $3, @$.line, @$.col);}
| expr '-' expr         {$$ = NODE(AST_MINUS,        $1, $3, @$.line, @$.col);}
| expr '*' expr         {$$ = NODE(AST_TIMES,        $1, $3, @$.line, @$.col);}
| expr '/' expr         {$$ = NODE(AST_DIVIDE,       $1, $3, @$.line, @$.col);}
| expr EQUALITY expr    {$$ = NODE(AST_EQUALITY,     $1, $3, @$.line, @$.col);}
| expr '<' expr         {$$ = NODE(AST_LESS_THAN,    $1, $3, @$.line, @$.col);}
| expr '>' expr         {$$ = NODE(AST_GREATER_THAN, $1, $3, @$.line, @$.col);}
;

%%

void yyerror (char const *err_msg)
{
	FIAL_parser_error_info->code = ERROR_PARSE;
	FIAL_parser_error_info->line = yylloc.line;
	FIAL_parser_error_info->col  = yylloc.col;
	FIAL_parser_error_info->static_msg = err_msg;
}
