#include <string.h>
#include <stdlib.h>

#include "interp.h"
#include "value_def_short.h"
#include "ast_def_short.h"
#include "ast.h"
#include "basic_types.h"
#include "error_macros.h"
#include "error_def_short.h"

/*these are needed for sanity, internal only so don't have to worry
 * about name conflicts.  So, functions prototypes in the header
 * aren't using them. */

typedef struct FIAL_exec_env              exec_env;
typedef struct FIAL_block                 block;
typedef struct FIAL_library               library;
typedef struct FIAL_ast_node              node;
typedef struct FIAL_value                 value;

typedef FIAL_symbol symbol;

#define lookup_symbol(x,y,z) FIAL_lookup_symbol(x,y,z)
#define set_symbol(x,y,z, w) FIAL_set_symbol(x,y,z, w)
#define create_symbol_map()    FIAL_create_symbol_map()

/*************************************************************/

#define FIAL_ALLOC(x)        malloc(x)
#define FIAL_REALLOC(x, y)   realloc(x, y)
#define FIAL_ALLOC_ERROR(x)  assert((x, 0))

#define ALLOC(x)             malloc(x)
#define FREE(x)              free(x)

#define SYMBOL_TABLE_START_SIZE 100
#define SYMBOL_LENGTH 31

#define ERROR(X) FIAL_INTERP_ERROR(X)
#define ALLOC_ERROR(x) ERROR(x)

/*
 * Still not sure about the best place to put this thing...
 */

void FIAL_set_error(struct FIAL_exec_env *env)
{
	E_SET_ERROR(*env);
}

/*symbols will all be the same length, and SYMBOL_LENGTH + 1 will
 * always be allocated, with symbol[SYMBOL_LENGTH] \0 */

/*************************************************************/

/* actually this is a special case, error wise.  It will just have to
 * use return codes.  */

/* returns -1 on bad alloc.*/

int FIAL_find_symbol(FIAL_symbol *sym,
		     const char *text,
		     struct FIAL_interpreter *interp)
{
	int i;
	struct FIAL_master_symbol_table *st = &(interp->symbols);

	*sym = 0;
	if(!interp)
		return 1;
	if(!text)
		return 1;

	for(i = 1; i < st->size; i++) {
		/*array should not contain null pointers*/
		assert(st->symbols[i]);
		if(strncmp(st->symbols[i], text, SYMBOL_LENGTH) == 0) {
			*sym = i;
			return 0;
		}
	}
	return 1;
}

int FIAL_get_symbol(FIAL_symbol *sym,
		    const char *text,
		    struct FIAL_interpreter *interp)
{
	char *new_sym;
	struct FIAL_master_symbol_table *st = &(interp->symbols);

	if(interp->state == FIAL_INTERP_STATE_RUN)
		return FIAL_find_symbol(sym, text, interp);

	if(st->symbols == NULL) {
		st->symbols =
			calloc(SYMBOL_TABLE_START_SIZE,
				   sizeof(char *));
		if(!st->symbols) {
			return -1;
		}
		st->cap = SYMBOL_TABLE_START_SIZE;
		st->size = 1;
		st->symbols[0] = NULL;  /*0 is not a valid symbol*/
	} else if (st->cap == st->size) {
		assert(st->symbols);
		char **tmp;
		tmp = FIAL_REALLOC(st->symbols, st->cap * 2 * sizeof(*st->symbols));
		if(!tmp){
			return -1;
		}
		st->symbols = tmp;
		memset(st->symbols + st->cap, 0, sizeof(*st->symbols)
		                                * st->cap);
		st->cap = st->cap * 2;

	}
	assert(st->symbols);
	if(FIAL_find_symbol(sym, text, interp) == 0) {
		return 0;
	}
	*sym = st->size;
	new_sym = FIAL_ALLOC(SYMBOL_LENGTH + 1);
	strncpy(new_sym, text, SYMBOL_LENGTH);
	new_sym[SYMBOL_LENGTH] = '\0';
	st->symbols[st->size++] = new_sym;
	return 1;
}

/*
 *  Would be better to handle this without copying.  But for now I am
 *  just going to do everything with my little symbol map structure,
 *  which doesn't have an interface for references just now.  No point
 *  in adding it either really, it is just a stop gap measure.
 */

static inline int lookup_symbol_value (value *val, symbol sym, exec_env *env)
{
	value v1;
	block *iter = env->block_stack;
	for(; iter != NULL; iter = iter->next) {
		if(!iter->values)
			continue;
		int failure = lookup_symbol(&v1, iter->values, sym);
		if(!failure ) {
			*val = v1;
			return 0;
		}
	}
	memset(val, 0, sizeof(*val));
	return 1;
}

static inline int declare_variable (node *stmt, exec_env *env)
{
	value tmp, v1;
	int failure =  0;
	node  *iter = NULL;

	assert(env->block_stack);
	memset(&tmp, 0, sizeof(tmp));

	if(!env->block_stack->values) {
		assert(env->block_stack->values == NULL);
		env->block_stack->values = create_symbol_map();
		if(!env->block_stack->values) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "Could not create needed value table";
			return -1;
		}
	}
	assert(env->block_stack->values);

/* I think I am taking this out, I am ok with allowing redeclaration
 * of variables within the same block.  I'm not worried about the
 * compiler, mangling variable names is not a big problem. But, under
 * the current implementation,  Allowing it would lead to bugs. */

	for(iter = stmt; iter!= NULL; iter=iter->right) {
		failure = lookup_symbol(&v1, env->block_stack->values,
					iter->sym);
		if(!failure) {
			env->error.code = ERROR_VARIABLE_REDECLARED;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "Attempt to redeclare "
    				"variable within the same "
				"block.";
			return -1;

		} else {
			set_symbol(env->block_stack->values, iter->sym, &tmp,
				   env);
		}
	}
	return 0;
}

static inline int eval_float_bi_op(value *val, char op, value *left, value *right)
{
	memset(val, 0, sizeof(*val));
	val->type = VALUE_FLOAT;
	switch(op) {
	case '+':
		val->x = left->x + right->x;
		break;
	case '-':
		val->x = left->x - right->x;
		break;
	case '*':
		val->x = left->x * right->x;
		break;
	case '/':
		val->x = left->x / right->x;
		break;
	case '<':
		val->type = VALUE_INT;
		val->n = (left->x < right->x);
		break;
	case '>':
		val->type = VALUE_INT;
		val->n = (left->x > right->x);
		break;
	default:
		assert(0);
		return -1;
		break;
	}
	return 0;
}


static inline int eval_int_bi_op(value *val, char op, value *left, value *right)
{
	memset(val, 0, sizeof(*val));
	val->type = VALUE_INT;
	switch(op) {
	case '+':
		val->n = left->n + right->n;
		break;
	case '-':
		val->n = left->n - right->n;
		break;
	case '*':
		val->n = left->n * right->n;
		break;
	case '/':
		val->n = left->n / right->n;
		break;
	case '<':
		val->n = (left->n < right->n);
		break;
	case '>':
		val->n = (left->n > right->n);
		break;
	default:
		assert(0);
		return -1;
		break;
	}
	return 0;
}
/*
error:
	env->error.code = ERROR_INVALID_EXPRESSION;
	env->error.line = stmt->loc.line;
	env->error.col  = stmt->loc.col;
	env->error.file = env->lib->label;
	end->env.static_msg = "numerical expresssion not involving floats or ints";

	return -1;

*/
static inline int eval_bi_op (char op,value *val, value *left, value *right)
{
	switch(left->type) {
	case VALUE_INT:
		switch(right->type) {
		case VALUE_INT:
			return eval_int_bi_op(val, op, left, right);
			break;
		case VALUE_FLOAT:
			left->type = VALUE_FLOAT;
			left->x = (float) left->n;
			return eval_float_bi_op(val, op, left, right);
			break;
		default:
			return -1;
			break;
		}
	case VALUE_FLOAT:
		switch(right->type) {
		case VALUE_INT:
			right->type = VALUE_FLOAT;
			right->x = (float) right->n;
			return eval_float_bi_op(val, op, left, right);
			break;
		case VALUE_FLOAT:
			return eval_float_bi_op(val, op, left, right);
			break;
		default:
			return -1;
			break;
		}
	default:
		return -1;
		break;
	}
	return 0;
}

/* I have to figure this out I guess, I probably will want a function
   like this in the api, but, here I ned something that the compiler can inline.  */

static inline int is_true (value *val)
{
	return (val->type == VALUE_INT   && val->n) ||
	       (val->type == VALUE_FLOAT && val->x);
}

static inline int eval_expression(value *val, node *expr, exec_env *env)
{
	int tmp;
	value left, right;

/* Not strictly speaking necessary I don't think, just don't want to
 * deal with uninitiated stuff right now. */

	tmp = 0;
	memset(val, 0, sizeof(*val));
	memset(&left, 0, sizeof(*val));
	memset(&right, 0, sizeof(*val));

	if(expr->left)
		if((tmp = eval_expression(&left, expr->left, env)) < 0)
			return tmp;
	if (expr->right)
		if((tmp = eval_expression(&right, expr->right, env)) < 0)
			return tmp;

	switch (expr->type)  {
	case AST_INT:
		val->type = VALUE_INT;
		val->n = expr->n;
		break;
	case AST_FLOAT:
		val->type = VALUE_FLOAT;
		val->x    = expr->x;
		break;
	case AST_SYMBOL:
		val->type = VALUE_SYMBOL;
		val->sym  = expr->sym;
		break;
	case AST_EXPR_ID:
		tmp = lookup_symbol_value(val, expr->sym, env);
		switch(val->type) {
		case VALUE_NONE:
		case VALUE_ERROR:
		case VALUE_INT:
		case VALUE_FLOAT:
		case VALUE_SYMBOL:
		case VALUE_STRING:
			return tmp;
			break;
		default:
			memset(val, 0, sizeof(*val));
			val->type = VALUE_ERROR;
			return 1;
			break;
		}
		break;
	case AST_STRING:
		val->type = VALUE_STRING;
		val->str  = expr->str;
		break;
	case AST_NEG:
		switch(left.type) {
		case VALUE_INT:
			*val = left;
			val->n *= -1;
			break;
		case VALUE_FLOAT:
			*val = left;
			val->x *= -1;
			break;
		default:
			env->error.code = ERROR_INVALID_EXPRESSION;
			env->error.line = expr->loc.line;
			env->error.col  = expr->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "numerical expresssion not "
				               "involving floats or ints";
			return -1;
			break;
		}
		break;

		/* note: the EGE macro is defined in error_macros.h,
		 * it means "on Error, Goto Error" with "error" being
		 * a magic goto label, given below.  */

	case AST_PLUS:
		EGE(eval_bi_op('+', val, &left, &right));
		break;
	case AST_MINUS:
		EGE(eval_bi_op('-', val, &left, &right));
		break;
	case AST_TIMES:
		EGE(eval_bi_op('*', val, &left, &right));
		break;
	case AST_DIVIDE:
		EGE(eval_bi_op('/', val, &left, &right));
		break;
	case AST_GREATER_THAN:
		EGE(eval_bi_op('>', val, &left, &right));
		break;
	case AST_LESS_THAN:
		EGE(eval_bi_op('<', val, &left, &right));
		break;
	case AST_EQUALITY:
		val->type = VALUE_INT;
		tmp = memcmp(&left, &right, sizeof(left));
		val->n = !tmp;
		break;
	case AST_AND:
		val->type = VALUE_INT;
		val-> n = (is_true(&left) && is_true(&right)) ? 1 : 0;
		break;
	case AST_OR:
		val->type = VALUE_INT;
		val-> n = (is_true(&left) || is_true(&right)) ? 1 : 0;
		break;
	case AST_NOT:
		val->type = VALUE_INT;
		val-> n = is_true(&left) ? 0 : 1;
		break;
	default:
		assert(0);
		break;
	}
	return 0;

error:

	env->error.code = ERROR_INVALID_EXPRESSION;
	env->error.line = expr->loc.line;
	env->error.col  = expr->loc.col;
	env->error.file = env->lib->label;
	env->error.static_msg = "numerical expresssion error, likely bad types.";

	return -1;

}

static inline int set_env_symbol (exec_env *env, symbol sym, value *val)
{
	value tmp;
	block *iter = env->block_stack;

	memset(&tmp, 0, sizeof(tmp));

	for(; iter != NULL; iter = iter->next) {
		if(!iter->values)
			continue;
		int result = lookup_symbol(&tmp, iter->values, sym);
		if(result == 0) {
			set_symbol(iter->values, sym, val, env);
			return 0;
		}
	}

	return -1;
}


static inline int assign_variable(node *stmt, exec_env *env)
{
	int tmp;
	value val;
	eval_expression(&val, stmt->left, env);

	if((tmp = set_env_symbol (env, stmt->sym, &val)) < 0) {

		env->error.code = ERROR_UNDECLARED_VAR;
		env->error.line = stmt->loc.line;
		env->error.col  = stmt->loc.col;
		env->error.file = env->lib->label;
		env->error.static_msg = "attempt to set undeclared variable.";
	}

	return tmp;
}

static inline int push_block (exec_env *env, node *block_node)
{
	block *b = FIAL_ALLOC(sizeof(block));

	if(!b)
		return -1;

	memset(b, 0, sizeof(*b));

	if(block_node->sym)
		b->label = block_node->sym;
	b->body_node = block_node->left;
	b->stmt = block_node->left->left;
	b->next = env->block_stack;
	env->block_stack = b;

	return 0;
}

static inline int execute_if (node *stmt, exec_env *env)
{
	int ret;
	value val;

	ret = eval_expression(&val, stmt->left, env);
	if(ret  < 0) {
		return ret;
	} else {
		ret = 0;
	}
	if(is_true(&val)) {
		ret = push_block(env, stmt->right);
		if(ret >= 0) {
			env->skip_advance = 1;
			return 0;
		}
		env->error.code = ERROR_BAD_ALLOC;
		env->error.line = stmt->loc.line;
		env->error.col  = stmt->loc.col;
		env->error.file = env->lib->label;
		env->error.static_msg = "unable to push block.";

		return ret;
	}
	return 1;
}

static inline int get_ref_from_sym_value_table(value *ref, symbol sym,
					       struct FIAL_symbol_map *map)
{
	struct FIAL_symbol_map_entry *iter;
	for(iter = map->first; iter != NULL; iter = iter->next) {
		if(iter->sym == sym) {
			memset(ref, 0, sizeof(*ref));
			ref->type = VALUE_REF;
			if(iter->val.type == VALUE_REF)
			    ref->ref = iter->val.ref;
			else
			    ref->ref = &iter->val;
			return 0;
		}
	}
	return 1;
}

static inline int get_ref_from_sym_block_stack (value *ref, symbol sym, block *bs)
{
	block *iter;
	for(iter = bs; iter != NULL; iter= iter->next) {
		if(iter->values) {
			int result =
				get_ref_from_sym_value_table(ref, sym,
							 iter->values);
			if (result == 0) {
				return 0;
			}
		}
	}

	memset(ref, 0, sizeof(*ref));
	return 1;
}

/*
returns

-1 for : allocation
-2 for bad lookup

ERROR("unbound identifier in arglist");

*/

static inline int insert_args (node *arglist_to, node *arglist_from,
			       struct FIAL_symbol_map *map_to, block *block_stack,
			       exec_env *env)
{
	node *iter1, *iter2;
	value val;
	memset(&val, 0, sizeof(val));

	iter1 = arglist_to;
	iter2 = arglist_from;

	for(; iter1 != NULL; iter1=iter1->right) {
		if(iter2) {
			value ref;
			int res = get_ref_from_sym_block_stack(&ref, iter2->sym,
							       block_stack);
			if(res == 1) {
				return -2;
			}
			res = set_symbol(map_to, iter1->sym, &ref, env);
			if(res == -1) {
				return -1;
			}
			iter2 = iter2->right;
		} else {
			int res = set_symbol(map_to, iter1->sym, &val, env);
			if(res == -1) {
				return -1;
			}
		}
	}
	return 0;
}

static inline int pop_block (exec_env *env)
{


	block *b = env->block_stack;
	env->block_stack = b->next;

	assert(b);
	if(b->last_lib)
		env->lib = b->last_lib;

//	FIAL_destroy_symbol_map(b->values, env->interp);


	if(b->values) {

		struct FIAL_symbol_map_entry *iter = NULL, *tmp = NULL;
		struct FIAL_finalizer *fin       = NULL;
		struct FIAL_finalizer *finishers = env->interp->types.finalizers;

		assert(finishers);

		for(iter = b->values->first; iter != NULL; iter = tmp) {
			tmp = iter->next;
			assert(iter->val.type < env->interp->types.size);
			fin = env->interp->types.finalizers + iter->val.type;
			if(fin->func) {
				fin->func(&iter->val, env->interp, fin->ptr);
			}
			free(iter);
		}
		free(b->values);
	}
	free(b);
	return 0;
}

/* returns:

-1 for bad alloc:
FIAL_ALLOC_ERROR("couldn't allocate a needed block");

FIAL_ALLOC_ERROR("couldn't create symbol map to hold "
"interpreter values.");

-2 for bad arg:
ERROR("unbound identifier in arglist");
*/

int perform_call_on_node (node *proc, node *arglist,
			  library *lib, exec_env *env)
{
	int res;
	block *last_block = env->block_stack;
	block *b = FIAL_ALLOC(sizeof(block));

	if(!b) {

		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "couldn't allocate block for proc call.";
		E_SET_ERROR(*env);
		return -1;
	}

	memset(b, 0, sizeof(*b));
	b->label = proc->sym;
	b->body_node = proc->right;
	b->stmt = proc->right->left;
	b->next = env->block_stack;
	env->block_stack = b;
	env->skip_advance = 1;

	if(arglist) {
		b->values = create_symbol_map();
		if(!b->values) {
			pop_block(env);
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "couldn't create value table "
				                "for new block in proc call";
			E_SET_ERROR(*env);
			return -1;
		}
		if((res = insert_args(proc->left, arglist,
				      b->values, last_block, env) < 0)) {
			pop_block(env);
			if(res == -1) {
				env->error.code = ERROR_BAD_ALLOC;
				env->error.static_msg = "bad alloc setting "
					"arguments";
				E_SET_ERROR(*env);
				return -1;
			} else {
				assert(res == -2);
				env->error.code = ERROR_UNDECLARED_VAR;
				env->error.static_msg = "undeclared variable "
							"in arglist";
				E_SET_ERROR(*env);
				return -1;
			}
		}
	}
	if(lib) {
		b->last_lib = env->lib;
		env->lib = lib;
	}
	return 0;
}

/* returns -1 on allocation error.
   return  -2 if a reference could not be found to match an argument.
*/

static inline int generate_external_arglist (int *argc, struct FIAL_value ***argvp,
					     node *arglist, exec_env *env)
{

	/*FIXME: make it so the parser counts how many arguments there
	 * are.*/
	int count;
	node *iter;
	int i;
	int tmp;

	(*argvp) = NULL;

	for(count = 0, iter = arglist; iter != NULL; iter = iter->right)
		count++;
	*argc = count;

	if(count) {
		(*argvp) = ALLOC(sizeof(struct FIAL_value *) * count);
	/* no need to initialize.... */
		if(!(*argvp)) {
			return -1;
		}
		for(i = 0, iter = arglist; i < count; i++, iter = iter->right) {
			value val;
			memset(&val, 0, sizeof(val));
			assert(iter);
			assert(iter->sym);
			tmp = get_ref_from_sym_block_stack(&val, iter->sym,
						     env->block_stack);
			if(tmp == 1) {
				FREE(*argvp);
				*argvp = NULL;
				return -2;
			}
			assert(val.type == VALUE_REF);
			(*argvp)[i] = val.ref;
		}
		return 0;
	}
	return 1;  /* no args... not sure if this matters.  */
}

/* ok this decision logic has to change, this gets split into 3
 * functions, one for external, one for internal, and one to tell the
 * difference.  the third function I guess is interp_call_B, which
 * already exists. */

/* ok, this has obviously become useless.... */

#ifdef THIS_ROUTINE_IS_DUMB
static inline int interp_call_on_proc (struct FIAL_proc *proc, node *arglist,
					library *lib, exec_env *env)
{
	return perform_call_on_node(proc->node, arglist, lib, env);
}
#endif /*THIS_ROUTINE_IS_DUMB*/

static inline int interp_call_on_func (struct FIAL_c_func     *func,
				       node                *arglist,
				       exec_env                *env)
{
	int argc;
	struct FIAL_value **argv = NULL;
	int err;
	int ret;

	err = generate_external_arglist(&argc, &argv, arglist, env);
	if(err == -1) {
		FREE(argv);
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "couldn't allocate space for "
					"external_arglist";
		E_SET_ERROR(*env);
		return -1;
	}
	if(err == -2) {
		FREE(argv);
		env->error.code = ERROR_UNDECLARED_VAR;
		env->error.static_msg = "unbound variable when "
					"generating argumennts";
		E_SET_ERROR(*env);
		return -2;
	}

	assert(func);
	assert(func->func);
	ret = func->func(argc, argv, env,func->ptr);

	FREE(argv);

	return ret;
}

static inline int execute_call_A (node *stmt, exec_env *env)
{
	value this_proc;
	int res;

	assert(env->lib);
	res = FIAL_lookup_symbol(&this_proc, env->lib->procs, stmt->sym);
	if(res == 1) {

		env->error.code = ERROR_UNKNOWN_PROC;
		env->error.line = stmt->loc.line;
		env->error.col  = stmt->loc.col;
		env->error.file = env->lib->label;
		env->error.static_msg = "attempt to call unknown proc.";
		return -1;

	}
	assert(this_proc.type = VALUE_NODE);

	/* call on node handles its own errors using the current stmt
	 * in env -- this is for parrellelism with
	 * interp_call_on_func, which has to handle errors itself, in
	 * order not to clobber errors from a function.  */

	return  perform_call_on_node(this_proc.node, stmt->left, NULL, env);
}

/* holy comolie this is needlessly complicated. and redundant.  and it
 * repeats itself.
 */

static inline int execute_call_B (node *stmt, exec_env *env)
{
	value proc;
	value lib;
	int res = 1;

	if(env->interp->omnis) {
		res = lookup_symbol(&lib, env->interp->omnis, stmt->sym);
	}
	if(res == 1) {
		assert(env->lib);
		if(!env->lib->libs) {
			env->error.code = ERROR_UNKNOWN_LIB;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to call a proc"
			                        " in an undeclared_lib";
			return -1;
		}
		res = lookup_symbol(&lib, env->lib->libs, stmt->sym);
		if(res == 1) {
			env->error.code = ERROR_UNKNOWN_PROC;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to access undeclared lib";
			return -1;
		}
	}
	if(lib.type == VALUE_LIB) {
		res = lookup_symbol(&proc, lib.lib->procs,
				    stmt->left->sym);
		if(res == 1) {
			env->error.code = ERROR_UNKNOWN_PROC;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to call unknown proc";
			return -1;
		}
		assert(proc.type == VALUE_NODE);
		return  perform_call_on_node(proc.node, stmt->right, lib.lib, env);
	} else {
		assert(lib.type == VALUE_C_LIB);
		res = lookup_symbol(&proc, lib.c_lib->funcs, stmt->left->sym);
		if(res == 1) {
			env->error.code = ERROR_UNKNOWN_PROC;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to call unknown function";
			return -1;
		}
		return interp_call_on_func(proc.func, stmt->right, env);

	}
	assert(0);
	return -1;
}

/*
static inline int execute_call_C (node *stmt, exec_env *env)
{
	value proc;
	value name;

	eval_expression(&name, stmt->left, env);

	if(name.type != VALUE_SYMBOL) {
		ERROR("Attempt to call a proc using an expression which does "
		      "not evaluate to a symbol.");
		return -1;
	}

	int res = lookup_symbol(&proc, env->lib->procs, name.sym);
	if(res == 1) {
		ERROR("attempt to call unknown proc.");
		return -1;
	}
	assert(proc.type = VALUE_PROC);

	return interp_call_on_proc(proc.proc, stmt->right, NULL, env);
}

static inline int execute_call_D (node *stmt, exec_env *env)
{
	value proc;
	value lib;
	value name;
	int res = 1;

	eval_expression(&name, stmt->left->left, env);
	if(name.type != VALUE_SYMBOL) {
		env->error.code =
		ERROR("Attempt to call a proc using an expression which does "
		      "not evaluate to a symbol.");
		return -1;
	}
	if(env->interp->omnis) {
		res = lookup_symbol(&lib, env->interp->omnis, name.sym);
	}
	if(res == 1) {
		if(!env->lib->libs) {
			ERROR("attempt to call a proc in a nonexistent lib.");
			return -1;
		}
		res = lookup_symbol(&lib, env->lib->libs, name.sym);
	}
	if(res == 1) {
		ERROR("attempt to access undeclared lib.");
		return -1;
	}
	assert(lib.type = VALUE_LIB);

	eval_expression(&name, stmt->left->right, env);
	if(name.type != VALUE_SYMBOL) {
		ERROR("Attempt to call a proc using an expression which does "
		      "not evaluate to a symbol.");
		return -1;
	}

	res = lookup_symbol(&proc, ((library *)lib.ptr)->procs,
				name.sym);
	if(res == 1) {
		ERROR("attempt to call unknown proc.");
		return -1;
	}
	assert(proc.type = VALUE_PROC);

	return interp_call_on_proc(proc.proc, stmt->right, lib.ptr, env);
}
*/

#define ENTRY_FREE(x) free(x)
#define MAP_FREE(x)   free(x)
#define finish_value(x,y,z) FIAL_finish_value(x,y,z)

/* ok, attach the lib and the interp yourself.... */


/*
  note:

  This is not really the recommended way to do this anymore, since the
  structures have been simplified (and are intended to remain simple).


*/

int FIAL_finish_value (value                   *val,
		       struct FIAL_finalizer   *fin,
		       struct FIAL_interpreter *interp)
{
	if(!fin)
		return 0;
	if(fin->func)
		return fin->func(val, interp, fin->ptr);
	return 0;
}


static inline int execute_break(node *stmt, exec_env *env)
{
	symbol sym = 0;

	if(stmt->sym == 0)
		return pop_block(env);
	do {
		sym = env->block_stack->label;
		ER(pop_block(env));
	} while (sym != stmt->sym && env->block_stack);
	return 0;
}

static inline int execute_continue(node *stmt, exec_env *env)
{
	struct FIAL_symbol_map *tmp;

	if(stmt->sym == 0) {

		if((tmp = env->block_stack->values)) {
			/*FIXME: this should probably get inlined, but
			  it can't now because it's in a different
			  file*/
			FIAL_destroy_symbol_map(tmp, env->interp);
			env->block_stack->values = NULL;
		}
		env->block_stack->stmt = env->block_stack->body_node->left;
		env->skip_advance = 1;
		return 0;
	}
	while(env->block_stack) {
		if(env->block_stack->label == stmt->sym) {
			if((tmp = env->block_stack->values)) {
				/*FIXME: this should probably get
				  inlined, but it can't now because
				  it's in a different file*/
				FIAL_destroy_symbol_map(tmp, env->interp);
				env->block_stack->values = NULL;
			}
			env->block_stack->stmt =
				env->block_stack->body_node->left;
			env->skip_advance = 1;
			return 0;
		}
		pop_block(env);
	}
	return 0;
}

int FIAL_interpret (struct FIAL_exec_env *env)
{
	block *b = NULL;
	node *stmt = NULL;

	if(!env || !env->block_stack)
		return -1;
	if(!env->block_stack->stmt)
		return 1;


	for(;;) {
		b = env->block_stack;
		stmt = b->stmt->left;

		assert(stmt);
		switch(stmt->type) {
		case AST_VAR_DECL:
			ER(declare_variable(stmt, env));
			break;
		case AST_ASSIGN:
			ER(assign_variable(stmt, env));
			break;
		case AST_IF:
			ER(execute_if(stmt, env));
			break;
		case AST_BLOCK:
			ER(push_block(env, stmt));
			env->skip_advance = 1;
			break;
		case AST_CALL_A:
			ER(execute_call_A(stmt, env));
			break;
		case AST_CALL_B:
			ER(execute_call_B(stmt, env));
			break;

/* I have taken these out of the language spec for now, I might put
 * them back in later, but I haven't really used them at all.  They
 * might prove useful later though.  I am just asserting, they aren't
 * produced by the parser anymore.  */

		case AST_CALL_C:
//			ER(execute_call_C(stmt, env));
			assert(0);
			break;
		case AST_CALL_D:
//			ER(execute_call_D(stmt, env));
			assert(0);
			break;
		case AST_BREAK:
			ER(execute_break(stmt, env));
			break;
		case AST_CONTINUE:
			ER(execute_continue(stmt, env));
			break;
		default:
			assert(0);
			/*is this an error?  I could just skip unknown
			 * statements...*/
			break;
		}
		if(!env->block_stack)
			break;
		if(env->skip_advance)
			env->skip_advance = 0;
		else
			env->block_stack->stmt = env->block_stack->stmt->right;
		while(!env->block_stack->stmt) {
			pop_block(env);
			if(!env->block_stack) {
				return 0;
			}
			assert(env->block_stack->stmt);
			env->block_stack->stmt = env->block_stack->stmt->right;
		}
	}
	return 0;
}
