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

static int map_dp_wrapper (struct FIAL_value *a,
		       struct FIAL_interpreter *b,
		       void   *c)
{
	return FIAL_destroy_symbol_map_value(a, b);
}

static int array_dp_wrapper (struct FIAL_value *a,
			     struct FIAL_interpreter *b,
			     void   *c)
{
	return FIAL_destroy_array_value(a, b);
}

static struct FIAL_finalizer map_fin   = {map_dp_wrapper,   NULL};
static struct FIAL_finalizer array_fin = {array_dp_wrapper, NULL};

#define INITIAL_TYPE_TABLE_SIZE 50
#if VALUE_USER > INITIAL_TYPE_TABLE_SIZE
#error VALUE_USER cannot be larger than INITIAL_TYPE_TABLE_SIZE
#endif /*VALUE_MAP >= INITIAL_TYPE_TABLE_SIZE*/

int init_types (struct FIAL_master_type_table *mtt)
{
	size_t size;

	mtt->finalizers = calloc((size = sizeof(*(mtt->finalizers))),
				  INITIAL_TYPE_TABLE_SIZE);
	if(!mtt->finalizers) {
		return -1;
	}

	mtt->cap = INITIAL_TYPE_TABLE_SIZE;
	mtt->size = VALUE_USER;

	assert(size);
	memset(mtt->finalizers, 0, size);

	mtt->finalizers[VALUE_MAP]   = map_fin;
	mtt->finalizers[VALUE_ARRAY] = array_fin;

/*
 * Add extra type finishers here.
 */
	return 0;
}
int FIAL_set_interp_to_run(struct FIAL_interpreter *interp)
{
	if(!interp)
		return -1;
	interp->state = FIAL_INTERP_STATE_RUN;
	return 0;
}

static void destroy_FIAL_lib (struct FIAL_library *lib)
{
	struct FIAL_symbol_map_entry *iter, *tmp;

	if(!lib)
		return;

	if(lib->procs) {
		for(iter = lib->procs->first; iter != NULL; iter = tmp) {
			tmp = iter->next;
			assert(iter->val.type == VALUE_NODE);
			FIAL_destroy_ast_node(iter->val.node);
			free(iter);
		}
		free(lib->procs);
	}

	/* globals always have simple destructors.... */
	if(lib->global) {
		for(iter = lib->global->first; iter != NULL; iter = tmp) {
			tmp = iter->next;
			free(iter);
		}
		free(lib->global);
	}
	/* libraries are not deleted recursively */
	if(lib->libs) {
		for(iter = lib->libs->first; iter != NULL; iter = tmp) {
			tmp = iter->next;
			free(iter);
		}
		free(lib->libs);
	}
	free(lib);
}

static void destroy_FIAL_c_lib (struct FIAL_c_lib *c_lib)
{
	if(!c_lib)
		return;

	struct FIAL_symbol_map_entry *iter, *tmp;
	if(c_lib->funcs) {
		for(iter = c_lib->funcs->first; iter != NULL; iter = tmp) {
			tmp = iter->next;
			free(iter);
		}
		free(c_lib->funcs);
	}
	free(c_lib);
}

static void destroy_FIAL_lib_entry (union FIAL_lib_entry *lib_ent)
{
	if(lib_ent->type == FIAL_LIB_FIAL) {
		destroy_FIAL_lib(&lib_ent->lib);
	} else {
		assert(lib_ent->type == FIAL_LIB_C);
		destroy_FIAL_c_lib (&lib_ent->c_lib);
	}
}

void FIAL_deinit_interpreter (struct FIAL_interpreter *interp)
{
	union FIAL_lib_entry *lib_iter, *lib_tmp;
	struct FIAL_symbol_map_entry *sym_iter, *sym_tmp;

	if(!interp)
		return;

	for(lib_iter = interp->libs; lib_iter != NULL; lib_iter = lib_tmp) {
		lib_tmp = lib_iter->stub.next;
		destroy_FIAL_lib_entry(lib_iter);
	}
	if(interp->omnis) {
		for(sym_iter = interp->omnis->first; sym_iter != NULL;
		    sym_iter = sym_tmp) {
			sym_tmp  = sym_iter->next;
			free(sym_iter);
		}
		free(interp->omnis);
	}
	if(interp->constants) {
		for(sym_iter = interp->constants->first; sym_iter != NULL;
		    sym_iter = sym_tmp) {
			sym_tmp  = sym_iter->next;
			free(sym_iter);
		}
		free(interp->constants);
	}

	free(interp->symbols.symbols);
	free(interp->types.finalizers);
	return;
}

void FIAL_destroy_interpreter(struct FIAL_interpreter *interp)
{
	FIAL_deinit_interpreter(interp);
	free(interp);
}

int FIAL_init_interpreter (struct FIAL_interpreter *interp)
{
	memset(interp, 0, sizeof(*interp));
	interp->state = FIAL_INTERP_STATE_LOAD;
	return init_types(&interp->types);
}

struct FIAL_interpreter *FIAL_create_interpreter (void)
{
	struct FIAL_interpreter *interp = malloc(sizeof(*interp));
	if(!interp)
		return NULL;
	if(FIAL_init_interpreter(interp) < 0) {
		free (interp);
		return NULL;
	}
	return interp;
}

static void reverse_block_stack (struct FIAL_block **bs)
{
	struct FIAL_block *iter, *tmp;
	struct FIAL_block *ns;  /* new stack */

	iter = *bs;
	ns = NULL;

	for(; iter != NULL; iter = tmp) {
		tmp = iter->next;
		iter->next=ns;
		ns = iter;
	}
	*bs = ns;
	return;
}

int FIAL_unwind_exec_env(struct FIAL_exec_env *env)
{
	struct FIAL_block *ns;   /* new_stack */
	struct FIAL_block *tmp;

	ns = env->block_stack;
	reverse_block_stack (&ns);
	for(; ns != NULL; ns = tmp) {
		tmp = ns->next;
		if(ns->values)
			FIAL_destroy_symbol_map(ns->values, env->interp);
		free(ns);
	}
	env->block_stack = NULL;
	return 0;
}

void FIAL_deinit_exec_env(struct FIAL_exec_env *env)
{
	/*
	 * actually, suprisingly, the exec_env does not actually seem
	 * to hold much in the way of actual data, outside of the
	 * block_stack.  I don't know if this is good or bad, but it
	 * was never really intended, the idea was this wouold have a
	 * bunch of stuff...
         */

	if(env->block_stack)
		FIAL_unwind_exec_env (env);
	free(env->error.dyn_msg);
	return;
}
void FIAL_destroy_exec_env(struct FIAL_exec_env *env)
{
	FIAL_deinit_exec_env(env);
	free(env);
}
int FIAL_init_exec_env(struct FIAL_exec_env *env)
{
	memset(env, 0, sizeof(*env));
	return 0;
}

struct FIAL_exec_env *FIAL_create_exec_env(void)
{
	struct FIAL_exec_env *env;
	env = malloc(sizeof(*env));
	if(!env)
		return NULL;
	if(FIAL_init_exec_env(env) < 0) {
		free (env);
		return NULL;
	}
	return env;
}

static int set_arguments_on_node (struct FIAL_ast_node *node,
				  struct FIAL_value *args,
				  struct FIAL_exec_env *env)
{
	struct FIAL_ast_node *iter;
	struct FIAL_value none;

	memset(&none, 0, sizeof(none));

	for(iter = node->left; iter != NULL; iter = iter->right) {
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
	return 0;
}

static int set_arguments_for_func (int *argc_ptr, struct FIAL_value ***argv_ptr,
				   struct FIAL_value *args)
{
	struct FIAL_value **argv;
	int argc = 0;

	if(!args) {
		argv = NULL;
		argc = 0;
	} else {
		int count, i;
		struct FIAL_value *iter;

		for(iter = args, count = 0;
		    iter->type != VALUE_END_ARGS;
		    ++iter, ++count)
			; /* just counting */

		if(count == 0) {
			argv = NULL;
			argc = 0;
		} else {
			argc = count;
			argv = calloc (sizeof(argv[0]), count);
			if(!argv)
				return -1; /* bad alloc */
			for(i = 0, iter = args; i < count; i++, ++iter)
				argv[i] = iter;
		}
	}

	*argc_ptr = argc;
	*argv_ptr = argv;
	return 0;
}

int FIAL_set_proc_from_strings(struct FIAL_proc *proc,
			       const char *lib_label,
			       const char *proc_name,
			       struct FIAL_interpreter *interp)
{
	FIAL_symbol sym;
	union FIAL_lib_entry *lib;
	struct FIAL_value val;
	struct FIAL_symbol_map *map = NULL;

	memset(&val, 0, sizeof(val));
	memset(proc, 0, sizeof(*proc));

	proc->interp = interp;
	if(FIAL_load_lookup(interp, lib_label, &lib))
		return -1;
	proc->lib = lib;
	if(lib->type == FIAL_LIB_FIAL) {
		proc->type = FIAL_PROC_FIAL;
		map = lib->lib.procs;
	} else {
		assert (lib->type == FIAL_LIB_C);
		proc->type = FIAL_PROC_C;
		map = lib->c_lib.funcs;
	}
	if(!sym)
		return -3;

	FIAL_get_symbol(&sym, proc_name, interp);
	FIAL_lookup_symbol(&val, map, sym);
	if((proc->type == FIAL_PROC_FIAL && val.type == VALUE_NODE) ||
	   (proc->type == FIAL_PROC_C    && val.type == VALUE_C_FUNC)) {
		proc->proc = val;
		return 0;
	} else {
		memset (proc, 0, sizeof(*proc));
		return -1;
	}
}

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
	return FIAL_run_ast_node (val.node, args, env);
}

/* I should change this to just have an error input for the return.*/

int FIAL_run_proc(struct FIAL_proc *proc,
		  struct FIAL_value *args,
		  struct FIAL_exec_env *env)
{
	if(!proc)
		return -1;

	env->interp = proc->interp;
	if(proc->type == FIAL_PROC_FIAL) {
		env->lib = &proc->lib->lib;
		perform_call_on_node(proc->proc.node,NULL, NULL, env);
		env->skip_advance = 0;
		set_arguments_on_node(proc->proc.node, args, env);
		if( FIAL_interpret(env) < 0 ) {
			return -1;
		} else {
			return 0;
		}
	} else  {
		int argc;
		struct FIAL_value **argv;

		if(set_arguments_for_func(&argc, &argv, args) < 0)
			return -2;
		assert(proc->proc.type == VALUE_C_FUNC &&
		       proc->proc.func && proc->proc.func->func);
		if(proc->proc.func->func(argc, argv, env,
					  proc->proc.func->func) < 0)
			return -1;
		return 0;
	}
	assert(0);
	return 0; /* just pretend it worked I guess */
}


/*
 * this name isn't ideal, but it is what I have right now.  I think
 * more descriptive names would be better, now that I have various
 * types of object that correspond to the concept of "procedure", with
 * a FIAL_ast_node not necessarily the most obvious.
 */

int FIAL_run_ast_node (struct FIAL_ast_node  *proc,
		       struct FIAL_value     *args,
		       struct FIAL_exec_env  *env)
{
	int ret;

	perform_call_on_node(proc, NULL, NULL, env);
	assert(env->block_stack);

	env->skip_advance = 0;

	set_arguments_on_node(proc, args, env);

	ret =  FIAL_interpret(env);
	return ret;
 }
