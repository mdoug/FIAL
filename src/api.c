/*
 * this file is for the external, top level, C api.  Will include
 * constructors/destructors for structs meant to be held by the C
 * program eventually.
 */

#include <string.h>

#include "interp.h"
#include "api.h"
#include "ast.h"
#include "ast_def_short.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "error_def_short.h"
#include "loader.h"

/*
 * this takes the library's label, and a string representation of its
 * a procedure's name, in that order.
 *
 * on success, this will overwrite the value of the env's lib.
 *
 * FIXME: these should be general purpose, i.e. call C funcs also, but
 * for now they only work on FIAL procs.
 */

int FIAL_run_strings (const char *lib_label,
		      const char *proc_name,
		      struct FIAL_value  *args,
		      struct FIAL_exec_env *env)
{
	FIAL_symbol sym;
	union FIAL_lib_entry *lib;
	struct FIAL_value val;

	memset(&val, 0, sizeof(val));

	if(!FIAL_load_lookup(env->interp, lib_label, &lib))
		return -1;
	if(lib->type != FIAL_LIB_FIAL) {
		assert(0);  /*FIXME */
		return -2;
	}
	FIAL_get_symbol(&sym, proc_name, env->interp);
	if(!sym)
		return -3;

	env->lib = &lib->lib;
	FIAL_lookup_symbol(&val, env->lib->procs, sym);
	if(val.type != VALUE_NODE)
		return -4;
	return FIAL_run_proc (val.node, args, env);
}

/*
 * this name isn't ideal, but it is what I have right now.  I think
 * more descriptive names would be better, now that I have various
 * types of object that correspond to the concept of "procedure", with
 * a FIAL_ast_node not necessarily the most obvious.
 */

int FIAL_run_proc (struct FIAL_ast_node  *proc,
		   struct FIAL_value     *args,
		   struct FIAL_exec_env  *env)
{
	int ret;
	struct FIAL_value none;

	memset(&none, 0, sizeof(none));

	perform_call_on_node(proc, NULL, NULL, env);
	assert(env->block_stack);

	env->skip_advance = 0;

	struct FIAL_ast_node *iter;
	for(iter = proc->left; iter != NULL; iter = iter->right) {
		if(args) {
			struct FIAL_value ref;
			ref = none;
			ref.type = VALUE_REF;
			ref.ptr = args;

			if(!env->block_stack->values) {
				env->block_stack->values =
					FIAL_create_symbol_map();
				if(!env->block_stack->values) {
					env->error.code = ERROR_BAD_ALLOC;
					env->error.static_msg =
						"Couldn't allocate to start "
						"interpreting;";
					return -1;
				}
			}

			FIAL_set_symbol(env->block_stack->values, iter->sym,
					&ref, env);

			if((++args)->type == VALUE_END_ARGS) {
				args = NULL;
			}
		} else {
			FIAL_set_symbol(env->block_stack->values, iter->sym,
					&none, env);
		}
	}

	ret =  FIAL_interpret(env);
	return ret;
}
