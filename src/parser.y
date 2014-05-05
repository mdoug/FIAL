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
    void yyerror(YYLTYPE, char const *);

    extern int yylineno;

    node *top = NULL;
    int FIAL_parser_line_no = 1;
    int FIAL_parser_col_no  = 1;

    struct FIAL_error_info *FIAL_parser_error_info;
%}

/* yacc doesn't support this initial action thing, it's hardly
   necessary.  The values can be initialized using yyextra when the
   parser becomes reentrant.  */

/* %initial-action {@$.line = @$.col = 1;}*/

/*
%locations
%pure-parser
*/

/*  for now I am using floats.  On 64 bit machines, doubles will make
 *  more sense, but floats would work there as well.
 */

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

%token VAR IF LEAVE REDO AND NOT OR DOUBLE_COLON UNKNOWN

%type <ast_node> expr assign statement statements body block redo if leave
%type <ast_node> arg_decl proc_def var_decl call top labelled_body common_body
%type <ast_node> initializer seq_init_items seq_init_item map_init_items map_init_item
%type <ast_node> map_access assign_left arglist arglist_item non_id_expr var_decl_item

%left AND OR NOT /* not sure where these should go, or what there
		    precedence should be in relation to one another,
		    but this seems more or less right. */
%left '<' '>' EQUALITY
%left '+' '-'
%left '*' '/'
%left NEG

%%

very_top:
top              {top = $1;}
;

top:
proc_def         {node *tmp = NODE(AST_TOP_NODE, $1, NULL, @$.line, @$.col);
		  $$ = NODE(AST_TOP, tmp, tmp, @$.line, @$.col); }
| top proc_def   {$$ = $1;
                  $$->right->right = NODE(AST_TOP_NODE, $2, NULL, @$.line, @$.col);
	          $$->right = $$->right->right;}
;

proc_def:
ID body                    {$$ = NODE(AST_PROC_DEF, NULL, $2, @$.line, @$.col);  $$->sym = $1;}
| ID '(' arg_decl ')' body {$$ = NODE(AST_PROC_DEF, $3, $5, @$.line, @$.col);  $$->sym = $1;}
;

/* this stores the last item in the list in the first items ->left field. */
arg_decl:
/*EMPTY*/               {$$ = NULL;}
| ID                    {$$ = NODE(AST_ARG_DECL, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| arg_decl ',' ID       {node *tmp = NODE(AST_ARG_DECL, NULL, NULL, @$.line, @$.col);
                         $$ = $1;  tmp->sym = $3;
               		 if(!$$->left) {
     			 	$$->right = $$->left = tmp;
			 } else {
				$$->left->right = tmp;
				$$->left = $$->left->right;
			 }
			 }
;

body:
'{'  common_body      {$$ = $2;}
;

/* This sneaks the label in on the statements node's value */

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
;

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
VAR var_decl_item              {$$ = NODE(AST_VAR_DECL,  $2, $2, @$.line, @$.col); $$->n = 1;}
| var_decl ',' var_decl_item   {$$ = $1; $$->right->right = $3; $$->right = $3;}
;

var_decl_item:
ID                       {$$ = NODE(AST_VAR_DECL_ITEM, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| ID '=' expr            {$$ = NODE(AST_VAR_DECL_ITEM,   $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID '=' initializer     {$$ = NODE(AST_VAR_DECL_ITEM,   $3, NULL, @$.line, @$.col); $$->sym = $1;}

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
;

arglist:
arglist_item                   {$$ = NODE(AST_ARGLIST, $1, $1, @$.line, @$.col);  $$->n = 1;}
| arglist ',' arglist_item     {$$ = $1; $$->n++; $$->right->right = $3;  $$->right = $3;}
;

/* for now, if you want to pass the evaluated value of variable,
   simplest is to enclose it in parenthesies -- proc_call((my_var)) */

arglist_item:
ID                         {$$ = NODE(AST_ARGLIST_ID,       NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| non_id_expr              {$$ = NODE(AST_ARGLIST_EXPR,     $1,   NULL, @$.line, @$.col);}
| initializer              {$$ = NODE(AST_ARGLIST_INIT,     $1,   NULL, @$.line, @$.col);}
| ':' ID                   {$$ = NODE(AST_ARGLIST_MOVE_ID,  NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| DOUBLE_COLON ID          {$$ = NODE(AST_ARGLIST_COPY_ID,  NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| ':' map_access           {$$ = NODE(AST_ARGLIST_MOVE_ACS, $2,   NULL, @$.line, @$.col);}
| DOUBLE_COLON map_access  {$$ = NODE(AST_ARGLIST_COPY_ACS, $2,   NULL, @$.line, @$.col);}
;

/* ok, the expression is being moved to the left, so that the map
 * accessor thing can be on the right.
 */

assign:
assign_left '=' expr               {$$ = $1; $$->right = $3;}
| assign_left '=' initializer      {$$ = $1; $$->right = $3;}
;

assign_left:
ID                     {$$ = NODE(AST_ASSIGN, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| map_access           {$$ = NODE(AST_ASSIGN,   $1, NULL, @$.line, @$.col);}
;

initializer:
/*
'{'  '}'                        {$$ = NULL;}
| '{' ':' '}'                   {$$ = NULL;}
| '{' ',' '}'                   {$$ = NULL;}
*/
'{' map_init_items  '}'       {$$ = $2;}  /* map */
| '{' seq_init_items  '}'       {$$ = $2;}  /* seq */
/*
| '{' ID DOUBLE_COLON expr '}'  {$$ = NULL;}
| '{' expr '}' {$$ = NULL;}
| '{' expr ',' expr '}' {$$ = NULL;}
| '{' DOUBLE_COLON expr '}' {$$ = NULL;}
| '{' '}'   {$$ = NULL;}
*/
;

map_init_items:
map_init_item                        {$$ = NODE(AST_MAP_INITIALIZER, $1, $1, @$.line, @$.col); $$->n = 1;}
| map_init_items ',' map_init_item   {$$ = $1; $$->right->right = $3; $$->right = $3; $$->n++;}
;

map_init_item:
ID '=' expr                  {$$ = NODE(AST_INIT_EXPRESSION,  $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID '=' initializer         {$$ = NODE(AST_INIT_INITIALIZER, $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID ':' map_access          {$$ = NODE(AST_INIT_MOVE_ACS,    $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID DOUBLE_COLON map_access {$$ = NODE(AST_INIT_COPY_ACS,    $3, NULL, @$.line, @$.col); $$->sym = $1;}
| ID ':' ID                  {node *tmp = NODE(AST_EXPR_ID,  NULL, NULL, @$.line, @$.col); tmp->sym = $3;
                              $$ = NODE(AST_INIT_MOVE_ID,    tmp, NULL, @$.line, @$.col); $$->sym = $1;}
| ID DOUBLE_COLON ID         {node *tmp = NODE(AST_EXPR_ID,  NULL, NULL, @$.line, @$.col); tmp->sym = $3;
                              $$ = NODE(AST_INIT_COPY_ID,     tmp, NULL, @$.line, @$.col); $$->sym = $1;}
;

seq_init_items:
seq_init_item                       {$$ = NODE(AST_SEQ_INITIALIZER, $1, $1, @$.line, @$.col); $$->n = 1;}
| seq_init_items ',' seq_init_item  {$$ = $1; $$->right->right = $3; $$->right = $3; $$->n++;}
;

seq_init_item:
expr                        {$$ = NODE(AST_INIT_EXPRESSION,    $1, NULL, @$.line, @$.col);}
| initializer               {$$ = NODE(AST_INIT_INITIALIZER,   $1, NULL, @$.line, @$.col);}
| ':' ID                    {$$ = NODE(AST_INIT_MOVE_ID,     NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| DOUBLE_COLON ID           {$$ = NODE(AST_INIT_COPY_ID,     NULL, NULL, @$.line, @$.col); $$->sym = $2;}
| ':' map_access            {$$ = NODE(AST_INIT_MOVE_ACS,      $2, NULL, @$.line, @$.col);}
| DOUBLE_COLON map_access   {$$ = NODE(AST_INIT_COPY_ACS,      $2, NULL, @$.line, @$.col);}
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

/*
 *  This little bit of wierdness is brought to you by the desire to
 *  have a bare id indicate passing a value by reference, in the case
 *  of a proc arg.
 */

expr:
non_id_expr             {$$ = $1;}
| ID                    {$$ = NODE(AST_EXPR_ID, NULL, NULL, @$.line, @$.col); $$->sym = $1;}
;

non_id_expr:
INT                     {$$ = NODE(AST_INT,     NULL, NULL, @$.line, @$.col); $$->n   = $1;}
| FLOAT                 {$$ = NODE(AST_FLOAT,   NULL, NULL, @$.line, @$.col); $$->x   = $1;}
| SYMBOL                {$$ = NODE(AST_SYMBOL,  NULL, NULL, @$.line, @$.col); $$->sym = $1;}
| STRING                {$$ = NODE(AST_STRING,  NULL, NULL, @$.line, @$.col); $$->str = $1;}
| map_access       {$$ = $1;}

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

    /* storing last element in the left of the first element.  I think
       at some point I have consider adding some consistency into
       this.  I should probably have all these variable list things
       have a header, which holds how many elements are in the list.*/

map_access:
ID '.' ID                  {node *tmp = NODE(AST_MAP_ACS, NULL, NULL, @3.line, @3.col);
			    $$ = NODE(AST_MAP_ACS , tmp, tmp, @1.line, @1.col);
			    $$->sym = $1; tmp->sym = $3;}
| map_access '.' ID        {$$ = $1;
                            $$->left->right = NODE(AST_MAP_ACS, NULL, NULL, @3.line, @3.col);
                            $$->left = $$->left->right; $$->left->sym = $3;}
;

%%

void yyerror (YYLTYPE loc, char const *err_msg)
{
	FIAL_parser_error_info->code = ERROR_PARSE;
	FIAL_parser_error_info->line = loc.line;
	FIAL_parser_error_info->col  = loc.col;
	FIAL_parser_error_info->static_msg = err_msg;
}
