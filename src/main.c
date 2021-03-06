#ifdef WIN32
#include <Windows.h>
#endif 

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
#include "sequence.h"


/*TODO -- set up a C_FUNC macro --
#define FIAL_C_FUNC(func_name)
*/

/*
 * the init code could certainly get cleaned up a tad.
 */


int FIAL_install_constants (struct FIAL_interpreter *);
int FIAL_install_std_omnis (struct FIAL_interpreter *);
int FIAL_install_proc (struct FIAL_interpreter *);
int FIAL_install_channels (struct FIAL_interpreter *);

int perform_call_on_node (struct FIAL_ast_node *proc,
			  struct FIAL_ast_node *arglist,
			  struct FIAL_library  *lib,
			  struct FIAL_exec_env *env);

void print_error (struct FIAL_error_info *err)
{
	printf("Error loading initial file %s on line %d, col %d:\n %s.\n" ,
	       err->file, err->line, err->col,
	       err->static_msg);
	if(err->dyn_msg)
		printf("%s.\n", err->dyn_msg);
}


/* this should be rewritten to use the new api, this is just a
   hodgepodge of stuff, most of which is no longer intended for top
   level use. 

   This code should be exemplary.
*/

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
	struct FIAL_proc         fp;

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
	memset(&err, 0, sizeof(err));

	FIAL_init_exec_env(&env);

	FIAL_install_constants(interp);
	FIAL_install_std_omnis(interp);
	FIAL_install_text_buf(interp);
	FIAL_install_seq(interp);
	FIAL_install_proc(interp);
	FIAL_install_channels(interp);

	/*	FIAL_install_system_lib (interp);*/

	ret = FIAL_load_label  (interp, filename, &lib_ent, &err );
	if(ret == -1) {
		print_error(&err);
		return -1; /* no need to free, we're done....*/
	}

/* run load function */
	if (FIAL_set_proc_from_strings(&fp, filename, "load", interp) >= 0) {
		if((ret = FIAL_run_proc(&fp, NULL, &env)) < 0) {
			printf("error in load function.\n");
			print_error(&env.error);
			return 1;
		}
	}
	FIAL_deinit_exec_env(&env);
	FIAL_init_exec_env(&env);

	FIAL_set_interp_to_run(interp);

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

	FIAL_deinit_exec_env(&env);
	FIAL_destroy_interpreter(interp);

	return ret;
}
