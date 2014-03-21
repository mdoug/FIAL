#ifndef FIAL_INTERP_H
#define FIAL_INTERP_H

/* used in current implementation of ERROR macros.  */
#include <assert.h>

#include "basic_types.h"

/*************************************************************
 *
 * Defines
 *
 *************************************************************/

#define FIAL_INTERP_ERROR(x) assert((x, 0))

/*************************************************************
 *
 * Declarations
 *
 *************************************************************/

/*
 * Defined in this header.
 */

struct FIAL_library;
struct FIAL_exec_env;
struct FIAL_interpreter;
struct FIAL_finalizer;

/*
 * Defined elsewhere
 */

struct FIAL_value;
struct FIAL_symbol_map;
struct FIAL_ast_node;

/*************************************************************
 *
 * Prototypes
 *
 *************************************************************/

/* returns 0 for found symbol, 1 for new, -1 for error */

int FIAL_get_symbol(FIAL_symbol *,
		    const char *text,
		    struct FIAL_interpreter *);
int FIAL_interpret (struct FIAL_exec_env *env);
int FIAL_finish_value (struct FIAL_value       *val,
		       struct FIAL_finalizer   *fin,
		       struct FIAL_interpreter *interp);
int perform_call_on_node (struct FIAL_ast_node *proc,
			  struct FIAL_ast_node *arglist,
			  struct FIAL_library  *lib,
			  struct FIAL_exec_env *env);
void FIAL_set_error(struct FIAL_exec_env *env);

/*************************************************************
 *
 * Definitions
 *
 *************************************************************/

/* ok, this library stuff feels like voodoo.  I'm going to leave it as
 * it is though, because (a) anything else would be a little less
 * efficient, and (b) anything else would be a little more work.  A
 * different type of data structure will probably be needed, at which
 * point this can be redone/re-evaluated.
 *
 * pulling out the type tag out of the union is definitely possible,
 * but would harly improve the voodoo-esque nature of this.
 */

#define FIAL_LIB_FIAL      1
#define FIAL_LIB_C         2

union  FIAL_lib_entry;

struct FIAL_library {
	int                       type;
	char                    *label;
	union FIAL_lib_entry     *next;

	struct FIAL_symbol_map   *  procs;
	struct FIAL_symbol_map   * global;
	struct FIAL_symbol_map   *   libs;
};

struct FIAL_c_lib {
	int                      type;
	char                   *label;
	union FIAL_lib_entry    *next;

	struct FIAL_symbol_map *funcs;
};

struct FIAL_lib_stub {
	int                     type;
	char                  *label;
	union FIAL_lib_entry   *next;
};

union FIAL_lib_entry {
	int                        type;
	struct FIAL_library         lib;
	struct FIAL_c_lib         c_lib;
	struct FIAL_lib_stub       stub;
};

struct FIAL_block {
	FIAL_symbol label;
	struct FIAL_symbol_map         *values;
	struct FIAL_ast_node *body_node, *stmt;
/*for a block stack...*/
	struct FIAL_library *last_lib;
	struct FIAL_block *next;
};


/* state stuff, I haven't actually done anything with this though. */

#define FIAL_EE_STATE_INITIAL    0
#define FIAL_EE_STATE_RUNNING    1
#define FIAL_EE_STATE_YIELD      2

/* < 0 could signify error, no need for a seperate variable for error
 * codes.*/

#define FIAL_EE_STATE_ERROR     -1

struct FIAL_error_info{
    int                         code;
    int                         line;
    int                          col;
    char const                 *file;
    char const           *static_msg;
    char                    *dyn_msg;
};

struct FIAL_exec_env {
	int                                state;
	struct FIAL_interpreter          *interp;
	struct FIAL_block           *block_stack;
	struct FIAL_library                 *lib;  //active lib....
	struct FIAL_error_info             error;
	int skip_advance;
};

struct FIAL_master_symbol_table {
	int size; /*numb of symbols*/
	int cap;  /*capacity */
	char ** symbols;
};

/*
  ok, replacing the C api with something more managable, starting with
  functions.

  NOW function args are :

  int (*) (int argc, struct FIAL_value *argv[],
           struct FIAL_exec_env *env, void *ptr);

  No need for a function arg type, struct FIAL_value ** is good enough.
*/

struct FIAL_c_func_def {
	char            *name;
	int (*func) (int, struct FIAL_value **, struct FIAL_exec_env *, void *);
	void             *ptr;
};

struct FIAL_c_func {
	int (*func) (int argc, struct FIAL_value *argv[],
		     struct FIAL_exec_env *env, void *ptr);
	void *ptr;
};

/*
 * This includes the interpreter out of convenience, since containers
 * need the interpreter to call the finalizers on them.  But -- this
 * is only really a convenience for containers, and I'm not sure how
 * often users will be writing their own containers.  It would never
 * be necessary, since it could always be included in a struct pointed
 * to by the *ptr field.
 */

struct FIAL_finalizer {
	int (*func)(struct FIAL_value *,
		    struct FIAL_interpreter *,
		    void   *ptr);
	void *ptr;
};

struct FIAL_master_type_table {
	int size;
	int cap;
	struct FIAL_finalizer *finalizers;
};

#define FIAL_INTERP_STATE_NONE   0
#define FIAL_INTERP_STATE_LOAD   1
#define FIAL_INTERP_STATE_RUN    2

struct FIAL_interpreter {
	int                                    state;
	union  FIAL_lib_entry                  *libs;
	struct FIAL_symbol_map            *constants;
	struct FIAL_symbol_map                *omnis;
	struct FIAL_master_symbol_table      symbols;
	struct FIAL_master_type_table          types;
};

#endif /* FIAL_INTERP_H */
