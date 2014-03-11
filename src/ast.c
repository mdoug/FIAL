#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ast.h"
#include "ast_def_short.h"

#define ALLOC(X)        malloc(X)
#define ALLOC_ERROR(x)  assert((x, 0))
#define FREE(X)         free(X)
#define STRING_FREE(x)  free(x)

struct FIAL_ast_node *FIAL_create_ast_node(void)
{
	struct FIAL_ast_node *ret = ALLOC(sizeof(*ret));
	if(!ret) {
		ALLOC_ERROR("couldn't allocate node!");
		return NULL;
	}
	memset(ret, 0, sizeof(*ret));
	return ret;
}

struct FIAL_ast_node *FIAL_get_ast_node(int type,
					struct FIAL_ast_node *left,
					struct FIAL_ast_node *right)
{
	struct FIAL_ast_node *ret = FIAL_create_ast_node();
	if(!ret)
		return NULL;
	memset(ret, 0, sizeof(*ret));
	ret->type = type;
	ret->left = left;
	ret->right = right;
	return ret;
}

struct FIAL_ast_node *
FIAL_get_ast_node_with_loc (int type, struct FIAL_ast_node *left,
			    struct FIAL_ast_node *right, int line,
			    int col)
{
	struct FIAL_ast_node *ret = FIAL_get_ast_node (type, left, right);
	if(!ret)
		return NULL;
	ret->loc.line = line;
	ret->loc.col  = col;

	return ret;
}

int FIAL_destroy_ast_node(struct FIAL_ast_node *n)
{
	FIAL_destroy_ast_node(n->left);
	FIAL_destroy_ast_node(n->right);

	if(n->type == AST_STRING)
	    STRING_FREE(n->str);

	FREE(n);

	return 0;
}
