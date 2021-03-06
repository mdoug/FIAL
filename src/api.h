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

int FIAL_init_interpreter (struct FIAL_interpreter *interp);
struct FIAL_interpreter *FIAL_create_interpreter (void);
void FIAL_deinit_interpreter (struct FIAL_interpreter *interp);
void FIAL_destroy_interpreter (struct FIAL_interpreter *interp);

int FIAL_set_interp_to_run(struct FIAL_interpreter *interp);

int FIAL_init_exec_env(struct FIAL_exec_env *env);
void FIAL_deinit_exec_env(struct FIAL_exec_env *env);
struct FIAL_exec_env *FIAL_create_exec_env(void);
void FIAL_destroy_exec_env(struct FIAL_exec_env *env);

int FIAL_unwind_exec_env(struct FIAL_exec_env *env);

int FIAL_set_proc_from_strings(struct FIAL_proc *proc,
                               const  char *lib_label,
                               const  char *proc_name,
                               struct FIAL_interpreter *interp);

int FIAL_run_proc (struct FIAL_proc *proc,
                   struct FIAL_value *args,
                   struct FIAL_exec_env *env);

#ifdef USE_FIAL_RUN_STRINGS 
int FIAL_run_strings (const char *lib_label,
		      const char *proc_name,
		      struct FIAL_value  *args,
		      struct FIAL_exec_env *env);
#endif /* USE_FIAL_RUN_STRINGS */

int FIAL_run_ast_node (struct FIAL_ast_node  *proc,
		       struct FIAL_value     *args,
		       struct FIAL_exec_env  *env);

char *FIAL_get_str (struct FIAL_value *);

/* dealing with values */

void FIAL_clear_value(struct FIAL_value *val,
		      struct FIAL_interpreter *interp);
void FIAL_move_value (struct FIAL_value *to,
		      struct FIAL_value *from,
		      struct FIAL_interpreter *interp);
int FIAL_copy_value (struct FIAL_value *to,
		     struct FIAL_value *from,
		     struct FIAL_interpreter *interp);
char *FIAL_get_str(struct FIAL_value *val);
#endif /*FIAL_API_H*/
