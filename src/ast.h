#ifndef FIAL_AST_H
#define FIAL_AST_H

/*
 *  Auto-generated defines are in ast_defines.json, they are converted
 *  using the script in ../utils.  I will be working on making that
 *  more general, then I can use it for the value types enum as well.
 */

/* ultimateluy I will want to take out the location information and
   move it, but for now this is simplest.  The performance hit won't
   be bad until you are dealing with large programs and cache misses,
   etc, so for now there is not even a meaningful way to profile stuff
   like that, and it just adds complexity.

   I will probably be using helper functions on the env variable
   anyway, just because getting line information from the node in that
   thing would be tedious.

   Though, now it's down to literally 2 ints, so its not a big deal.
   It does increase the struct size by 50% though.

*/

struct FIAL_ast_node {
    int type;
    struct FIAL_ast_node *left, *right;
    union {
	int n;
	float x;
	int sym;
	char *str;
    };
    struct {
	int   line;
	int    col;
    } loc;
};

/*actually, not sure I want these here, these can be static in the
 * parser, except for destroy, which isn't used in the parser at
 * all, so I don't have to worry about it just yet */

struct FIAL_ast_node *FIAL_create_ast_node(void);
struct FIAL_ast_node *FIAL_get_ast_node(int type,
					struct FIAL_ast_node *left,
					struct FIAL_ast_node *right);
struct FIAL_ast_node *
FIAL_get_ast_node_with_loc (int type, struct FIAL_ast_node *left,
			    struct FIAL_ast_node *right, int line,  int col);

int FIAL_destroy_ast_node(struct FIAL_ast_node *);

#endif /*FIAL_INTERP_H*/
