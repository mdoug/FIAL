#ifndef FIAL_API_H
#define FIAL_API_H

#include "interp.h"
#include "basic_types.h"

/*
 * not sure this is the best place for this structure, it will be
 * needed for the libraries which deal directly with procs /
 * executions / threads.
 */

#define FIAL_PROC_FIAL 0
#define FIAL_PROC_C    1

/*
 * I think I am going to refactor this, but I need something to move
 * forward with.  Also not entirely sure I like the name, since
 * generally C functions are referred to as "funcs", and this is
 * general -- either FIAL proc or FIAL C FUNC.
 */

struct FIAL_proc {
	int type;
	struct FIAL_interpreter *interp;
	union  FIAL_lib_entry      *lib;
	struct FIAL_value          proc;
};

struct FIAL_interpreter *FIAL_create_interpreter (void);
int FIAL_init_exec_env(struct FIAL_exec_env *env);
struct FIAL_exec_env *FIAL_create_exec_env(void);

int FIAL_set_proc_from_strings(struct FIAL_proc *proc,
			       const  char *lib_label,
			       const  char *proc_name,
			       struct FIAL_interpreter *interp);

int FIAL_run_proc (struct FIAL_proc *proc,
		   struct FIAL_value *args,
		   struct FIAL_exec_env *env);

int FIAL_run_strings (const char *lib_label,
		      const char *proc_name,
		      struct FIAL_value  *args,
		      struct FIAL_exec_env *env);

int FIAL_run_ast_node (struct FIAL_ast_node  *proc,
		       struct FIAL_value     *args,
		       struct FIAL_exec_env  *env);

#endif /*FIAL_API_H*/
