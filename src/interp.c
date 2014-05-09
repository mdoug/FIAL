#include <string.h>
#include <stdlib.h>

#include "interp.h"
#include "value_def_short.h"
#include "ast_def_short.h"
#include "ast.h"
#include "basic_types.h"
#include "error_macros.h"
#include "error_def_short.h"
#include "sequence.h"
#include "api.h"

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

/*************************************************************
 *
 * Prototypes
 *
 *************************************************************/

static int handle_initializer  (value *val, node *init, exec_env *env);
static int get_initialized_seq(value *val, node *init, exec_env *env);
static int get_initialized_map(value *val, node *init, exec_env *env);
static int eval_expression(value *val, node *expr, exec_env *env);

/*************************************************************/

#define SYMBOL_TABLE_START_SIZE 100
#define SYMBOL_LENGTH 31

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
		char **tmp;

		assert(st->symbols);
		tmp = realloc(st->symbols, st->cap * 2 * sizeof(*st->symbols));
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
	new_sym = malloc(SYMBOL_LENGTH + 1);
	strncpy(new_sym, text, SYMBOL_LENGTH);
	new_sym[SYMBOL_LENGTH] = '\0';
	st->symbols[st->size++] = new_sym;
	return 1;
}

/*
 * DANGER!
 *
 * This returns a copy of the value, without calling the copy
 * procedure.  Do NOT leave both the returned value, and the original
 * value, in play, unless they are known to have default copiers.
 *
 * I don't have anything better than this, unfortunately.  It would
 * maybe be better to return the pointer to the original value, but
 * that would also require caution to use.  This proc makes sense in
 * the context of expression evaluation, so that's why it is the way
 * it is.
 */

static inline int lookup_symbol_value (value *val, symbol sym, exec_env *env)
{
	value v1;
	block *iter = env->block_stack;
	for(; iter != NULL; iter = iter->next) {
		int failure;

		if(!iter->values)
			continue;
		failure = lookup_symbol(&v1, iter->values, sym);
		if(!failure ) {
			*val = v1;
			return 0;
		}
	}
	memset(val, 0, sizeof(*val));
	return 1;
}

/*
 *  ok, I'm going to use this for the map accessors in move/copy type
 *  stuff.
 *
 *  Take care when rewriting data structures for the value table that
 *  changing this function doesn't break the code.
 */

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

/*I think I have to chill out with all the "inlines", especially on
  non-small functions that will likely be reused.

  these "get_ref" functions are returning 1 when no ref is found, so
  I'm going to keep that going.

  will return -1 in the event that something other than a map is being
  accessed.
*/
static  int get_ref_from_map_access (value *ref, node *map_access,
				     block *bs)
{
	value val, none;
	node *iter;

	assert(map_access);
	assert(map_access->right);
	assert(ref);

	memset(&none, 0, sizeof(none));
	if(get_ref_from_sym_block_stack(&val, map_access->sym, bs) != 0) {
		*ref = none;
		return 1;
	}
	for(iter = map_access->right; iter != NULL; iter = iter->right) {
		assert(val.type == VALUE_REF);
		if(val.ref->type != VALUE_MAP) {
			return -1;
		}
		if(get_ref_from_sym_value_table(&val,iter->sym,
						val.ref->map)!=0) {
			*ref = none;
			return 1;
		}
	}
	*ref = val;
	return 0;
}

/*
 * This returns a reference to a value that is contained within a
 * nested compound of maps and sequences.
 *
 * Not sure where else to put this, it requires the
 * get_ref_from_sym_value_table, which is static, but at the same
 * time it really should be in with the api functions.  It is mainly
 * for use by the omni.put/take/dupe functions, except that user
 * functions would likely want to reuse the interface for their own
 * code.  I suppose this is not terribly important, so it can just
 * chill here for now.
 */

/*
 * returns -1 if there was an access error.
 *
 *  Will probably improve this error reporting, this will probably not
 *  be good enough.
 */

int FIAL_access_compound_value (struct FIAL_value     *ref,
				struct FIAL_value     *compound,
				struct FIAL_value     *accessor)
{
	struct FIAL_value *acs;
	struct FIAL_value val;

	int i, count; /* simplest */


	assert(ref && compound && accessor);;
	switch (accessor->type) {
	case VALUE_INT:
	case VALUE_SYMBOL:
		acs = accessor;
		count = 1;
		break;
	case VALUE_SEQ:
		if (accessor->seq->first >= accessor->seq->head)
			return -1;
		count = accessor->seq->head - accessor->seq->first;
		acs = accessor->seq->first;
		break;
	default:
		return -1;
		break;
	}

	val.type = VALUE_REF;
	val.ref  = compound;

	for(i = 0; i < count; i++) {
		switch (acs[i].type) {
		case VALUE_SYMBOL:
			if(val.ref->type != VALUE_MAP) {
				return -1;
			}
			if (get_ref_from_sym_value_table(&val, acs[i].sym,
							 val.ref->map) != 0) {
				return -1;
			}
			break;
		case VALUE_INT:
			if(val.ref->type != VALUE_SEQ)
				return -1;
			if (acs[i].n < 1 ||
			    acs[i].n > (val.ref->seq->head -
				      val.ref->seq->first))
				return -1;

			val.ref = val.ref->seq->first + (acs[i].n - 1);
			break;
		default:
			return -1;
			break;
		}
	}
	*ref = val;
	return 0;
}


/* returns -1 on bad allot,
   returns -2 on error when initializing  */


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

/*
 * I think I am taking this out, I am ok with allowing redeclaration
 * of variables within the same block.  I'm not worried about the
 * compiler, mangling variable names is not a big problem. But, under
 * the current implementation,  Allowing it would lead to bugs.
 *
 * I forgot what bugs would be caused by this, unfortunately, I didn't
 * write it down as far as I can see.  Anyway, I am ok leaving this
 * until I redesign the main value stack data structure.
 */

	for(iter = stmt->left; iter!= NULL; iter=iter->right) {
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
			value val = tmp;
			if(iter->left) {
				if(iter->left->type == AST_SEQ_INITIALIZER ||
				   iter->left->type == AST_MAP_INITIALIZER) {
					if( handle_initializer
					    (&val, iter->left,env) < 0)
						return -1;
				} else {
					if(eval_expression(&val, iter->left,
							   env) < 0)
						return -1;
				}
			}
			if ( set_symbol(env->block_stack->values, iter->sym,
					&val, env) < 0) {
				env->error.code = ERROR_BAD_ALLOC;
				env->error.static_msg =
					"couldn't allocate space for new value";
				FIAL_set_error(env);
				return -1;
			}
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

/*
 * FIXME: divide by zero is set to VALUE error.  I don't know if this makes
 * sense.  I have to consider my options with regard to the expression
 * evaluation errors.    
 */

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
		if(right->n == 0) {
			val->type = VALUE_ERROR;
			break;
		}
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

static inline int access_map (value *val, node *map_access, exec_env *env)
{
	int tmp;
	node *iter;
	value Fv, *map;

	if((tmp = lookup_symbol_value(&Fv, map_access->sym, env)) != 0) {
		env->error.static_msg = "unknown map in expression";
		goto error;
	}
	if(Fv.type != VALUE_MAP) {
		env->error.static_msg = "attempt to access member of something "
		                        "that is not a map in an expression.";
		goto error;
	}

	iter = map_access->right;
	map = &Fv;

	while(iter->right) {
		if(FIAL_lookup_symbol(&Fv, map->map, iter->sym) != 0) {
			env->error.static_msg = "attempt to access member of something "
			                        "that is not a map in an expression.";
			goto error;
		}
		if(Fv.type != VALUE_MAP) {
			env->error.static_msg = "attempt to access member of something "
			                        "that is not a map in an expression.";
			goto error;
		}

		map = &Fv;
		iter = iter->right;
	}

	assert(iter);
	assert(iter->right == NULL);

	FIAL_lookup_symbol(val, map->map, iter->sym);
	return 0;

 error:
	env->error.code = ERROR_INVALID_EXPRESSION;
	env->error.line = map_access->loc.line;
	env->error.col  = map_access->loc.col;
	env->error.file = env->lib->label;
	return -1;
}

static int eval_expression(value *val, node *expr, exec_env *env)
{
	int tmp;
	value left, right;

/* Not strictly speaking necessary I don't think, just don't want to
 * deal with uninitiated stuff right now. */

	tmp = 0;
	memset(val,    0, sizeof(*val));
	memset(&left,  0, sizeof(*val));
	memset(&right, 0, sizeof(*val));


	/* have to handle this first, since it is a terminal, but it
	 * has a left and a right operand.  This way everything else
	 * can carry on unchanged.
	 *
	 * if this is going to get moved into the switch somehow, it's
	 * value in ast_defines.json has to be moved, so that the code
	 * will optomize into a jumptable.
	 */

	if(expr->type == AST_MAP_ACS) {
		if(access_map(val, expr, env) < 0)
			return -1;
		/* convert non expressable types to value_error */
		switch(val->type) {
		case VALUE_NONE:
		case VALUE_ERROR:
		case VALUE_INT:
		case VALUE_FLOAT:
		case VALUE_SYMBOL:
		case VALUE_STRING:
		case VALUE_TYPE:
			return 0;
			break;
		default:
			memset(val, 0, sizeof(*val));
			val->type = VALUE_ERROR;
			return 1;
			break;
		}
		assert(0);
		return 0;
	}
	if(expr->left) {
		if((tmp = eval_expression(&left, expr->left, env)) < 0)
			return tmp;
		if(left.type == VALUE_ERROR) {
			*val = left;
			return 1;
		}
	}
	if (expr->right) {
		if((tmp = eval_expression(&right, expr->right, env)) < 0)
			return tmp;
		if(right.type == VALUE_ERROR) {
			*val = right;
			return 1;
		}
	}
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
		if((tmp = lookup_symbol_value(val, expr->sym, env)) != 0) {
			env->error.code = ERROR_INVALID_EXPRESSION;
			env->error.line = expr->loc.line;
			env->error.col  = expr->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "unbound variable in expression";
			return -1;
		}
		switch(val->type) {
		case VALUE_NONE:
		case VALUE_ERROR:
		case VALUE_INT:
		case VALUE_FLOAT:
		case VALUE_SYMBOL:
		case VALUE_STRING:
		case VALUE_TYPE:
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
		int result; 

		if(!iter->values)
			continue;
		result = lookup_symbol(&tmp, iter->values, sym);
		if(result == 0) {
			set_symbol(iter->values, sym, val, env);
			return 0;
		}
	}

	return -1;
}



/*
 *  get_initialized_map / get_initialized_seq --
 *
 *  returns -1: bad alloc,
 *          -2: expr error.
 *          -3: unbound variable.
 *          -4: map access.
 */

enum init_type {INIT_SEQ, INIT_MAP};
static int get_initialized_value (const enum init_type, value *val, node *init,
				  exec_env *env);


static int get_initialized_seq(value *val, node *init, exec_env *env)
{
	return get_initialized_value(INIT_SEQ, val, init, env);
}

static int get_initialized_map(value *val, node *init, exec_env *env)
{
	return get_initialized_value(INIT_MAP, val, init, env);
}

static int get_initialized_value (const enum init_type init_type, value *val, node *init,
				  exec_env *env)
{
	int res;
	value tmp, tmp2, none;
	node *iter;

	assert (val && init && env && init->right && init->left);
	assert (init_type == INIT_MAP || init_type == INIT_SEQ);

	if(init_type == INIT_SEQ) {
		val->type = VALUE_SEQ;
		val->seq  = FIAL_create_seq();
		if(!val->seq)
			return -1;

	} else {
		val->type = VALUE_MAP;
		val->map  = FIAL_create_symbol_map();
		if(!val->map)
			return -1;
	}
	memset(&none, 0, sizeof(none));

/* this coding style is a bit inelegant, but it seems simplest.  If
 * there were more than two alternative initializers, I would prefer
 * this seperated out into different functions, this is about the
 * limit of the complexity of decision logic that I want to deal
 * with. */

	for(iter = init->left; iter != NULL; iter = iter->right) {

/* just initializing the buggers at the top of the loop, this could
 * maybe be skipped, but that would be an optomization that I don't
 * need to mess with right now.
 */
		tmp = tmp2 = none;
		switch(iter->type) {
		case AST_INIT_EXPRESSION:
			if(eval_expression(&tmp, iter->left, env) < 0) {
				FIAL_seq_in(val->seq, &tmp);
				return -2;
			}
			if (init_type == INIT_SEQ) {
				if( FIAL_seq_in(val->seq, &tmp) < 0) {
					return -1;
				}
			} else {
				if(FIAL_set_symbol(val->map, iter->sym,
						   &tmp, env)<0) {
					return -1;
				}
			}
			break;
		case AST_INIT_INITIALIZER:
			switch(iter->left->type) {
			case AST_SEQ_INITIALIZER:
				if((res = get_initialized_seq
				    (&tmp, iter->left, env)) < 0) {
					return res;
				}
				break;
			case AST_MAP_INITIALIZER:
				if((res = get_initialized_map
				    (&tmp, iter->left, env)) < 0) {
					return res;
				}
				break;
			default:
				assert(0);
				break;
			}
			if (init_type == INIT_SEQ) {
				if( FIAL_seq_in(val->seq, &tmp) < 0) {
					return -1;
				}
			} else {
				if(FIAL_set_symbol(val->map, iter->sym,
						   &tmp, env) < 0) {
					return -1;
				}
			}
			break;
		case AST_INIT_MOVE_ID:
			if (init_type == INIT_SEQ) {
				res = get_ref_from_sym_block_stack(&tmp, iter->sym,
								   env->block_stack);
				if(res != 0) {
					return -3;
				}
				if(FIAL_seq_in(val->seq, tmp.ref) < 0) {
					return -1;
				}
			} else {
				res = get_ref_from_sym_block_stack
					(&tmp, iter->left->sym, env->block_stack);
				if(res != 0) {
					return -3;
				}
				if(FIAL_set_symbol(val->map, iter->sym,
						   tmp.ref, env) < 0) {
					return -1;
				}
			}
			*tmp.ref = none;
			break;
		case AST_INIT_COPY_ID:
			if (init_type == INIT_SEQ) {
				res = get_ref_from_sym_block_stack
					(&tmp, iter->sym, env->block_stack);
				if (res != 0) {
					return -3;
				}
				tmp2 = none;
				if (FIAL_copy_value(&tmp2, tmp.ref, env->interp) < 0) {
					return -1;
				}
				if (FIAL_seq_in(val->seq, &tmp2) < 0) {
					FIAL_clear_value(&tmp2, env->interp);
					return -1;
				}
			} else {
				res = get_ref_from_sym_block_stack
					(&tmp, iter->left->sym, env->block_stack);
				if (res != 0) {
					return -3;
				}
				tmp2 = none;
				if (FIAL_copy_value(&tmp2, tmp.ref, env->interp) < 0) {
					return -1;
				}
				if (FIAL_set_symbol(val->map, iter->sym,
						    &tmp2, env)  < 0) {
					FIAL_clear_value(&tmp2, env->interp);
					return -1;
				}
			}
			break;
		case AST_INIT_MOVE_ACS:
			res = get_ref_from_map_access(&tmp, iter->left,
						      env->block_stack);
			if (res != 0)
				return -4;
			if (init_type == INIT_SEQ) {
				if( FIAL_seq_in(val->seq, tmp.ref) < 0) {
					return -1;
				}
			} else {
				if(FIAL_set_symbol(val->map, iter->sym,
						   tmp.ref, env) < 0) {
					return -1;
				}
			}
			*tmp.ref = none;
			break;
		case AST_INIT_COPY_ACS:
			res = get_ref_from_map_access(&tmp, iter->left,
						      env->block_stack);
			if (res != 0)
				return -4;
			tmp2 = none;
			if( FIAL_copy_value(&tmp2, tmp.ref, env->interp) < 0)
				return -1;
			if (init_type == INIT_SEQ) {
				if( FIAL_seq_in(val->seq, &tmp2) < 0) {
					FIAL_clear_value(&tmp2, env->interp);
					return -1;
				}
			} else {
				if(FIAL_set_symbol(val->map, iter->sym,
						   &tmp2, env)<0) {
					FIAL_clear_value(&tmp2, env->interp);
					return -1;
				}
			}
			break;
		default:
			assert(0);
			break;
		}
	}
	return 0;
}

static int handle_initializer  (value *val, node *init, exec_env *env )
{
	int res;
	int tmp;
	assert(val && init && env);
	tmp = init->type;

	if(tmp == AST_SEQ_INITIALIZER)
		res = get_initialized_seq(val, init, env);
	else
		res = get_initialized_map(val, init, env);
	if (res < 0) {
		switch (res) {
		case -1:
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "bad alloc when handling initializer";
			break;
		case -2:
			return -1;
			break;
		case -3:
			env->error.code = ERROR_UNDECLARED_VAR;
			env->error.static_msg = "unknown variable in initializer";
			break;
		case -4:
			env->error.code = ERROR_INVALID_MAP_ACCESS;
			env->error.static_msg = "unable to access map";
			break;
		}
		FIAL_set_error(env);
	}
	return res;
}

static inline int assign_variable(node *stmt, exec_env *env)
{
	int tmp, new_val;
	value val, var_val, Fv, *set_me;
	struct FIAL_ast_node *iter;

	if((tmp = stmt->right->type) == AST_SEQ_INITIALIZER ||
	   tmp == AST_MAP_INITIALIZER) {
		if( handle_initializer(&val, stmt->right, env) < 0) {
			return -1;
		}
	} else 	if (eval_expression(&val, stmt->right, env) < 0) {
		return -1;
	}
	if(!stmt->left) {
		if((tmp = set_env_symbol (env, stmt->sym, &val)) < 0) {
			env->error.code = ERROR_UNDECLARED_VAR;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to set undeclared variable.";
		}
		return tmp;
	}

	assert(stmt->left);

	/* see, I think I want to auto-mapify.  I feel like, in
	   general, I don't want to generate errors in the
	   interpreter, if there is a reasonable interpretation that
	   enables the program to continue executing.  This is in
	   contrast to my take on the libraries, where generally I
	   treat everything irregular as an error.

	   For now I will simply trigger an error, since that is
	   easiest to implement, and I have a lot of stuff to do, and
	   I don't want to get bogged down.

	   Changing later will be simple, and all the code that
	   detects whether or not the value is currently a map, has to
	   be in place in any case.

	*/
	if((tmp = lookup_symbol_value (&var_val, stmt->left->sym, env)) != 0) {
		env->error.code = ERROR_UNDECLARED_VAR;
		env->error.line = stmt->loc.line;
		env->error.col  = stmt->loc.col;
		env->error.file = env->lib->label;
		env->error.static_msg = "attempt to set undeclared variable.";

		return tmp;
	}

	iter = stmt->left->right;
	set_me = &var_val;
	new_val = 0;

	while(iter->right) {
		if(FIAL_lookup_symbol(&Fv, set_me->map, iter->sym) != 0) {
			env->error.code = ERROR_INVALID_MAP_ACCESS;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to set symbol on "
				"nonexistent map entry";
			return -1;
		}
		set_me =  &Fv;
		if(set_me->type != VALUE_MAP) {
			env->error.code = ERROR_INVALID_MAP_ACCESS;
			env->error.line = stmt->loc.line;
			env->error.col  = stmt->loc.col;
			env->error.file = env->lib->label;
			env->error.static_msg = "attempt to set symbol on "
				"something that is not a map.";
			return -1;
		}
		iter = iter->right;
	};
	assert(iter->right == NULL);
	assert(set_me->type == VALUE_MAP);

	FIAL_set_symbol(set_me->map, iter->sym, &val, env);

	return 0;
}

static inline int push_block (exec_env *env, node *block_node)
{
	block *b = malloc(sizeof(block));

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


/*
returns

-1 for : allocation
-2 for bad lookup
-3 for bad expression,
-4 for bad initializer.
-5 for bad map accessor

The args will be left on the incompleted block in the case of error.
Not sure what else to do about this, but it is a little unfortunate,
but it is currently always possible to tell which was the last value
pushed onto the block, so the one which caused the error would
necessarily be the one after that.

*/

static int insert_args (node *arglist_to, node *arglist_from,
			struct FIAL_symbol_map *map_to, block *block_stack,
			exec_env *env)
{

	node *iter1, *iter2;
	value val, ref, none;
	int res;
	memset(&none, 0, sizeof(val));

	iter1 = arglist_to;
	iter2 = arglist_from->left;

	for(; iter1 != NULL; iter1=iter1->right) {
		if(iter2) {
			res = 0;
			val = ref = none;
			switch(iter2->type) {
			case AST_ARGLIST_ID:
				res = get_ref_from_sym_block_stack
					(&ref, iter2->sym, block_stack);
				if(res == 1) {
					return -2;
				}
				res = set_symbol(map_to, iter1->sym, &ref, env);
				if(res == -1) {
					return -1;
				}
				break;
			case AST_ARGLIST_EXPR:
				assert(iter2->type == AST_ARGLIST_EXPR);

				if(eval_expression(&val, iter2->left,env) < 0) {
					return -3;
				}
				res = set_symbol(map_to, iter1->sym, &val, env);
				if (res == -1)
					return -1;
				break;
			case AST_ARGLIST_INIT:
				if(handle_initializer(&val, iter2->left,
						      env) < 0) {
					return -4;
				}
				res = set_symbol(map_to, iter1->sym, &val, env);
				if(res == -1)
					return -1;
				break;
			case AST_ARGLIST_MOVE_ID:
				res = get_ref_from_sym_block_stack(&ref,
				          iter2->sym, block_stack);
				if(res == 1) {
					return -2;
				}
				val = *ref.ref;
				res = set_symbol(map_to, iter1->sym, &val, env);
				if(res == -1) {
					return -1;
				}
				memset(ref.ref, 0, sizeof(*ref.ref));
				break;
			case AST_ARGLIST_COPY_ID:
				res = get_ref_from_sym_block_stack(&ref,
				          iter2->sym, block_stack);
				if(res == 1) {
					return -2;
				}
				FIAL_copy_value (&val, ref.ref, env->interp);
				res = set_symbol(map_to, iter1->sym, &val, env);
				if(res == -1) {
					FIAL_clear_value(&val, env->interp);
					return -1;
				}
				break;
			case AST_ARGLIST_MOVE_ACS:
				assert(iter2->left);
				res = get_ref_from_map_access(&ref, iter2->left,
							      env->block_stack);
				if (res != 0)
					return -5;
				res = set_symbol(map_to, iter1->sym,
						 ref.ref, env);
				if(res < 0)
					return -1;
				*ref.ref = none;
				break;
			case AST_ARGLIST_COPY_ACS:
				assert(iter2->left);
				res = get_ref_from_map_access(&ref, iter2->left,
							      env->block_stack);
				if(res != 0)
					return -5;
				if( FIAL_copy_value(&val, ref.ref, env->interp) < 0)
					return -1;
				if(set_symbol(map_to, iter1->sym, &val, env)
				   < 0) {
					return -1;
				}
				break;
			default:
				assert(0);
			}

			iter2 = iter2->right;
		} else {
			int res;	

			assert(!iter2);
			res = set_symbol(map_to, iter1->sym, &val, env);
			if(res == -1) {
				return -1;
			}
		}
	}
	return 0;
}

/*
 * This cannot possibly be the right place for the pop block function.
 */

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
	block *b = malloc(sizeof(block));

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
			FIAL_set_error(env);
			return -1;
		}
		if((res = insert_args(proc->left, arglist,
				      b->values, last_block, env)) < 0) {
			pop_block(env);
			switch(res) {
			case -1:
				env->error.code = ERROR_BAD_ALLOC;
				env->error.static_msg = "bad alloc setting "
					"arguments";
				FIAL_set_error(env);
				return -1;
			case -2:
				assert(res == -2);
				env->error.code = ERROR_UNDECLARED_VAR;
				env->error.static_msg = "undeclared variable "
							"in arglist";
				FIAL_set_error(env);
				return -1;
			case -5:
				env->error.code = ERROR_UNDECLARED_VAR;
				env->error.static_msg = 
				"Map access error when generating arglist";
				FIAL_set_error(env);
				return -1;
			default:
				assert(res == -3 || res == -4);
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

static inline void free_arg_strip (int count,
				   struct FIAL_value *arg_strip,
				   struct FIAL_interpreter *interp)
{
	int i;

	if(arg_strip == NULL)
		return;
	for(i = 0; i < count; i++)
		FIAL_clear_value(arg_strip + i, interp);
	free(arg_strip);
}

/* returns -1 on allocation error.
   return  -2 if a reference could not be found to match an argument.
           -3 on bad invalid expression.
	   -4 for invalid initializer
           -5 for bad map accessor

   this function currently frees all memory on error, which is not
   good for error reporting, since there will not be a record of which
   argument caused the error.

   This will become a problem when robust error reporting becomes
   standard, as it stands now, it is not much worse than what is the
   current norm for errors.
 */

static inline int generate_external_arglist (int *argc,
					     struct FIAL_value ***argvp,
					     node *arglist,
					     struct FIAL_value **arguments,
					     exec_env *env)
{
	/*FIXME: make it so the parser counts how many arguments there
	 * are.*/
	int result = 0;
	int count = 0;
	node *iter;
	int i, res;
	int tmp;
	value ref;

	value *arg_strip = NULL;
	(*argvp) = NULL;

/*
	for(count = 0, iter = arglist; iter != NULL; iter = iter->right)
		count++;
		*argc = count; */

	if(arglist) {
		assert(arglist->n);
		*argc = count = arglist->n;
		arglist = arglist->left;
		arg_strip = calloc(sizeof(*arg_strip), count);
		*arguments = arg_strip;
	}

	if(count) {
		(*argvp) = malloc(sizeof(struct FIAL_value *) * count);
	/* no need to initialize.... */
		if(!(*argvp)) {
			result = -1;
			goto error;
		}
		for(i = 0, iter = arglist; i < count; i++, iter = iter->right) {
			value val;
			memset(&val, 0, sizeof(val));
			assert(iter);

			switch(iter->type) {
			case  AST_ARGLIST_ID:
				assert(iter->sym);
				tmp = get_ref_from_sym_block_stack(&val,
					iter->sym, env->block_stack);
				if(tmp == 1) {
					result = -2;
					goto error;
				}
				assert(val.type == VALUE_REF);
				(*argvp)[i] = val.ref;
				break;
			case AST_ARGLIST_EXPR:
				assert(iter->type == AST_ARGLIST_EXPR);
				assert(arg_strip);
				tmp = eval_expression(arg_strip + i, iter->left,
						      env);
				if(tmp < 0) {
					result = -3;
					goto error;
				}
				(*argvp)[i] = arg_strip + i;
				break;
			case AST_ARGLIST_INIT:
				assert(iter->type == AST_ARGLIST_INIT);
				assert(arg_strip);
				tmp = handle_initializer(arg_strip + i,
							 iter->left,  env);
				if(tmp < 0) {
					result = -4;
					goto error;
				}
				(*argvp)[i] = arg_strip + i;
				break;
			case AST_ARGLIST_MOVE_ID:
				assert(arg_strip);
				res = get_ref_from_sym_block_stack(&ref,
				          iter->sym, env->block_stack);
				if(res == 1) {
					result = -2;
					goto error;
				}
				FIAL_move_value(arg_strip + i, ref.ref,
						env->interp);
				(*argvp)[i] = arg_strip + i;
				break;
			case AST_ARGLIST_COPY_ID:
				assert(arg_strip);

				res = get_ref_from_sym_block_stack(&ref,
				          iter->sym, env->block_stack);
				if(res == 1) {
					result =  -2;
					goto error;
				}
				if(FIAL_copy_value(arg_strip + i, ref.ref,
						   env->interp) < 0) {
					result = -1;
					goto error;
				}
				(*argvp)[i] = arg_strip + i;
				break;
			case AST_ARGLIST_MOVE_ACS:
				assert(iter->left);
				assert(arg_strip);

				res = get_ref_from_map_access(&ref,
				          iter->left, env->block_stack);
				if(res != 0) {
					result =  -5;
					goto error;
				}
				FIAL_move_value(arg_strip + i, ref.ref,
						env->interp);
				(*argvp)[i] = arg_strip + i;
				break;
			case AST_ARGLIST_COPY_ACS:
				assert(iter->left);
				assert(arg_strip);

				res = get_ref_from_map_access(&ref,
				          iter->left, env->block_stack);
				if(res != 0) {
					result =  -5;
					goto error;
				}
				if(FIAL_copy_value(arg_strip + i, ref.ref,
						   env->interp) < 0) {
					result = -1;
					goto error;
				}
				(*argvp)[i] = arg_strip + i;
				break;
			default:
				assert(0);
				break;
			}
		}
		return 0;
	}
	return 1;  /* no args... not sure if this matters.  */

error:
	free(*argvp);
	/* freeing is fine, since expressable values do not have
	 * finilizers. */
	free_arg_strip(count, arg_strip, env->interp );

	*argvp = NULL;
	*arguments = NULL;

	return result;
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
	int argc = 0;
	struct FIAL_value **argv = NULL;
	struct FIAL_value *arguments = NULL;
	int err;
	int ret;

	err = generate_external_arglist(&argc, &argv, arglist,
					&arguments, env);
	if(err < 0) {
		switch(err) {
		case -1:
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = 
			"couldn't allocate space for external_arglist";
			FIAL_set_error(env);
			ret =  -1;
			goto cleanup;
		case -2:
			env->error.code = ERROR_UNDECLARED_VAR;
			env->error.static_msg = "unbound variable when "
						"generating argumennts";
			FIAL_set_error(env);
			ret =  -2;
			goto cleanup;
		case -5:
			env->error.code = ERROR_INVALID_MAP_ACCESS;
			env->error.static_msg = "unbound variable when "
						"generating argumennts";
			FIAL_set_error(env);
			ret =  -5;
			goto cleanup;
		default:
			assert(err == -3 || err == -4);
			ret =  err;
			goto cleanup;
		}
	}

	assert(func);
	assert(func->func);
	ret = func->func(argc, argv, env,func->ptr);

cleanup:
	free(argv);
	/* freeing is ok, since all arguments are the results of
	 * expressions */
	free_arg_strip(argc, arguments, env->interp);

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
