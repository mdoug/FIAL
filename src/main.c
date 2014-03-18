#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "interp.h"
#include "loader.h"
#include "basic_types.h"
#include "value_def_short.h"

#include "ast.h"
#include "text_buf.h"


/*TODO -- set up a C_FUNC macro --
#define FIAL_C_FUNC(func_name)
*/

/*
 * the init code could certainly get cleaned up a tad.
 */


int FIAL_install_constants (struct FIAL_interpreter *);
int FIAL_install_std_omnis (struct FIAL_interpreter *);

int perform_call_on_node (struct FIAL_ast_node *proc,
			  struct FIAL_ast_node *arglist,
			  struct FIAL_library  *lib,
			  struct FIAL_exec_env *env);

/*
static inline void print_node_loc (struct FIAL_ast_node *loc)
{
	printf("line: %d, col %d\n", loc->loc.line, loc->loc.col);
}

static void print_tree_loc (struct FIAL_ast_node *tree)
{
	if(tree) {
		print_node_loc(tree);
		print_tree_loc(tree->left);
		print_tree_loc(tree->right);
	}
}
*/
int test_print_args (struct FIAL_interpreter *interp)
{
	struct FIAL_library   *lib;
	union  FIAL_lib_entry *lib_ent;
	struct FIAL_exec_env  env;

	struct FIAL_value args[3], val;
	struct FIAL_error_info err;
	int ret;
	FIAL_symbol sym;

	memset(args, 0, sizeof(args));
	memset(&env, 0, sizeof(env));
	memset(&err, 0, sizeof(err));

	ret = FIAL_load_string  (interp, "print.fial", &lib_ent, &err );
	if(ret == -1) {
		printf("Error loading initial file %s on line %d, col %d:\n %s.\n" ,
		       err.file, err.line, err.col,
			err.static_msg);
		if(err.dyn_msg)
		    printf("%s.\n", err.dyn_msg);
		return -1; /* no need to free, we're done....*/
	}

	lib = &lib_ent->lib;
	FIAL_get_symbol   (&sym,  "print_it", interp );
	FIAL_lookup_symbol(&val,  lib->procs, sym );

	assert(val.type == VALUE_NODE);

	env.interp = interp;
	env.lib    = lib;

	args[0].type = VALUE_INT;
	args[0].n    = 20;
	args[1].type = VALUE_STRING;
	args[1].str  = "Whodat!";
	args[2].type = VALUE_END_ARGS;

	return FIAL_run_ast_node(val.node, args, &env);
}

int main(int argc, char *argv[])
{
	struct FIAL_interpreter *interp = FIAL_create_interpreter();
	struct FIAL_library     *lib = NULL;
	union  FIAL_lib_entry   *lib_ent = NULL;
	struct FIAL_value       val;
	struct FIAL_exec_env    env;
	FIAL_symbol             sym;
	int                     ret;
	struct FIAL_error_info  err;

	char              *filename;
	char             *proc_name;

	if(argc != 3 && argc != 2) {
	    printf("Usage: interp file [proc]\nproc defaults to \"run\"");
	    return 1;
	}
	filename = argv[1];
	if(argc == 3)
	    proc_name = argv[2];
	else
	    proc_name = "run";

	memset(&val, 0, sizeof(val));
	memset(&env, 0, sizeof(env));
	memset(&err, 0, sizeof(err));

	FIAL_install_constants(interp);
	FIAL_install_std_omnis(interp);
	FIAL_install_text_buf(interp);
	FIAL_install_system_lib (interp);

	ret = FIAL_load_string  (interp, filename, &lib_ent, &err );
	if(ret == -1) {
		printf("Error loading initial file %s on line %d, col %d:\n %s.\n" ,
		       err.file, err.line, err.col,
			err.static_msg);
		if(err.dyn_msg)
		    printf("%s.\n", err.dyn_msg);
		return -1; /* no need to free, we're done....*/
	}

	lib = &lib_ent->lib;
	FIAL_get_symbol   (&sym,  proc_name, interp );
	FIAL_lookup_symbol(&val,  lib->procs, sym );

	if(val.type != VALUE_NODE) {
	    printf("could find proc %s\n", proc_name);
	    return 1;
	}

	lib = &lib_ent->lib;

	/*
	printf("in library %s:\n", lib->label);
	for(iter = lib->procs->first; iter != NULL; iter = iter->next) {
	    print_tree_loc(iter->val.proc);
	}
	*/

//	test_print_args(interp);

	env.interp = interp;
	env.lib    = lib;

	perform_call_on_node(val.node, NULL, NULL, &env);
	env.skip_advance = 0;

	ret = FIAL_interpret(&env);

/*


	ret = FIAL_run_proc(interp, lib, val.proc->node);

*/
	printf("interpret returned %d\n", ret);
	if(ret < 0)
		printf("Error in file %s on line %d, col %d: %s.\n",
		       env.error.file, env.error.line, env.error.col,
			env.error.static_msg);


	return ret;
}
