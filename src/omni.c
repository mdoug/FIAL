#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interp.h"
#include "api.h"
#include "ast.h"
#include "loader.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "error_macros.h"
#include "error_def_short.h"

#define ERROR(x) assert((x, 0))
#define ALLOC(x) malloc(x)

/*FIXME : this can be a static inline funciton, don't remmeber why i
  did it like this even though i just did it.*/

#define INSTALL_CONSTANT(name, val, interp) do{FIAL_symbol sym = 0; \
	FIAL_get_symbol(&sym, name, interp); \
	FIAL_set_symbol_raw(interp->constants, sym, &val);}while(0)

int FIAL_install_constants (struct FIAL_interpreter *interp)
{
	struct FIAL_value val = {0};

	if(!interp->constants) {
		interp->constants = FIAL_create_symbol_map();
		if(!interp->constants) {
			ERROR("bad alloc");
			return -1;
		}
	}

	memset(&val, 0, sizeof(val));
	INSTALL_CONSTANT("none", val, interp);

	val.type = VALUE_ERROR;
	INSTALL_CONSTANT("error", val, interp);

	return 0;
}

/* Note to self -- FIX THIS.  There is no reason this can't just be a
 * normal library!  but for now I'm just fixing it up a little bit.. */

static int get_constant(int argc, struct FIAL_value *argv[],
			struct FIAL_exec_env *env, void *ptr)
{
	assert(env);
	assert(env->interp);
	assert(env->interp->constants);
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "takes 2 arguments to retrieve "
                                        "constant: return value, and symbol.";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(argv);
	if(argv[1]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "arg2 to const must be a symbol.";
		E_SET_ERROR(*env);
		return -1;
	}
	FIAL_clear_value(argv[0], env->interp);
	return FIAL_lookup_symbol( argv[0],
				   env->interp->constants,
				   argv[1]->sym);
}

int print(int argc, struct FIAL_value **argv,
	       struct FIAL_exec_env *env,
	       void *ptr)
{
	int i;
	struct FIAL_value *ref;
	if( argc == 0)
		return 1;

	assert(argv);
	for(i = 0; i < argc; i++) {
		ref = argv[i];
		switch(ref->type) {
		case VALUE_NONE:
			printf("NONE ");
			break;
		case VALUE_INT:
			printf("INT: %d ", ref->n);
			break;
		case VALUE_FLOAT:
			printf("FLOAT: %f ", ref->x);
			break;
		case VALUE_TYPE:
			printf("TYPE object, n: %d ", ref->n);
			break;
		case VALUE_STRING:
			printf("%s ", ref->str);
			break;
		case VALUE_TEXT_BUF:
			assert(ref->text);
			if(!ref->text->buf)
				printf("(empty text buf) ");
			else
				printf("%s ", ref->text->buf);
			break;
		case VALUE_SYMBOL:
			printf("$%s ", env->interp->symbols.symbols[ref->sym]);
			break;
		default:
			printf("Object of type: %d\n", ref->type);
			break;
		}
	}
	printf("\n");
	return 0;
}

static int run_load_on_lib (struct FIAL_proc *proc,
			    struct FIAL_interpreter *interp,
			    struct FIAL_error_info  *error)
{
	int ret = 0;
	struct FIAL_exec_env env;

	FIAL_init_exec_env(&env);
	env.interp = interp;

	if ((ret = FIAL_run_proc(proc, NULL, &env)) < 0) {
		*error = env.error;
		env.error.dyn_msg = NULL;
	}

	FIAL_deinit_exec_env(&env);
	return ret;
}

static int load_lib (int argc, struct FIAL_value **args,
	      struct FIAL_exec_env *env,
	      void *ptr)
{
	union FIAL_lib_entry *lib_ent  = NULL;
	int ret                         = 0;
	struct FIAL_value val           = {0};
	int new_lib                     = 0;

	if(env->interp->state != FIAL_INTERP_STATE_LOAD) {
		env->error.code = ERROR_INTERP_STATE;
		env->error.static_msg = "Can only load libraries in load state.";
		FIAL_set_error(env);
		return -1;
	}

	if(argc < 2) {
		env->error.code  = ERROR_INVALID_ARGS;
		env->error.static_msg = "Attempting to load library without "
			                "2 arguments.";
		E_SET_ERROR(*env);
		return -1;
	}

	if(args[0]->type != VALUE_SYMBOL) {
		env->error.code  = ERROR_INVALID_ARGS;
		env->error.static_msg = "load requires a symbol as arg1 to "
			                "attach the library to!";
		E_SET_ERROR(*env);
		return -1;
	}
	if(args[1]->type != VALUE_STRING) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg ="load requires a string as arg2 "
				       "that names the library!";
		E_SET_ERROR(*env);
		return -1;
	}

	assert(env->lib);
	if(!env->lib->libs){
		env->lib->libs = FIAL_create_symbol_map();
		if (!env->lib->libs) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "couldn't allocate symbol map...";
			E_SET_ERROR(*env);
			return -1;
		}
	}

	if(FIAL_load_lookup (env->interp, args[1]->str, &lib_ent)) {
		new_lib = 0;
	} else {
		new_lib = 1;
		memset(&env->error, 0, sizeof(env->error));
		ret = FIAL_load_label(env->interp, args[1]->str,
				      &lib_ent, &env->error);
		if(ret < 0)
			return -1;
		memset(&env->error, 0, sizeof(env->error));
	}
	assert(lib_ent);
	if( new_lib && lib_ent->type == FIAL_LIB_FIAL ) {
		struct FIAL_proc fp;

		val.type  =  VALUE_LIB;
		val.lib   =  &lib_ent->lib;
		if( FIAL_set_proc_from_strings(&fp, lib_ent->lib.label, "load",
					       env->interp) >= 0) {
			if (run_load_on_lib(&fp,env->interp, &env->error) < 0) {
				return -1;
			}
		}
	} else  {
		if (lib_ent->type == FIAL_LIB_FIAL) {
			assert(!new_lib);
			val.type  =  VALUE_LIB;
			val.lib   =  &lib_ent->lib;
		} else {
			assert(lib_ent->type == FIAL_LIB_C);
			val.type   = VALUE_C_LIB;
			val.c_lib  = &lib_ent->c_lib;
		}
	}
	assert(env->lib->libs);

/* FIXME: this doesn't check if the symbol was overwritten--maybe that's ok. */

/* i'm really not sure this is the right set symbol, but it's fine for
 * now, all these structures are in a state of flux, unfortunately.*/
	FIAL_set_symbol(env->lib->libs, args[0]->sym, &val, env);
	return ret;
}

static int move_value (int argc, struct FIAL_value **argv,
		       struct FIAL_exec_env *env,
		       void *ptr)
{
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Moving requires 2 values, to and from";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_move_value(argv[0], argv[1], env->interp);
	return 0;
}

static int copy_value (int argc, struct FIAL_value **argv,
		       struct FIAL_exec_env *env,
		       void *ptr)
{
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Copying requires 2 values, to and from";
		FIAL_set_error(env);
		return -1;
	}

	if(FIAL_copy_value(argv[0], argv[1], env->interp) < 0) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "Could not perform copy.";
		FIAL_set_error(env);
		return -1;
	}

	return 0;
}

/* omni accessors -- put, take, and dupe */

static int omni_put (int argc, struct FIAL_value **args,
		     struct FIAL_exec_env *env,	void *ptr)

{
	enum {CONT = 0, ACCESSOR = 1, PUT_ME = 2};
	struct FIAL_value ref;
	(void) ptr;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Need 3 arguments to put value somewhere.";
		FIAL_set_error(env);
		return -1;
	}
	if(FIAL_access_compound_value(&ref, args[CONT], args[ACCESSOR]) < 0) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Error in arguments to access container";
		FIAL_set_error(env);
		return -1;
	}
	assert(ref.type == VALUE_REF);
	FIAL_move_value(ref.ref, args[PUT_ME], env->interp);
	return 0;
}

static int omni_take (int argc, struct FIAL_value **args,
		      struct FIAL_exec_env *env, void *ptr)

{
	enum {SET_ME, CONT, ACCESSOR};
	struct FIAL_value ref;
	(void) ptr;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Need 3 arguments to take value";
		FIAL_set_error(env);
		return -1;
	}
	if(FIAL_access_compound_value(&ref, args[CONT], args[ACCESSOR]) < 0) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Error in arguments to access container";
		FIAL_set_error(env);
		return -1;
	}
	assert(ref.type == VALUE_REF);
	FIAL_move_value(args[SET_ME], ref.ref, env->interp);
	return 0;
}

static int omni_dupe (int argc, struct FIAL_value **args,
		      struct FIAL_exec_env *env, void *ptr)

{
	enum {SET_ME, CONT, ACCESSOR};
	struct FIAL_value ref;
	(void) ptr;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Need 3 arguments to take value";
		FIAL_set_error(env);
		return -1;
	}
	if(FIAL_access_compound_value(&ref, args[CONT], args[ACCESSOR]) < 0) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Error in arguments to access container";
		FIAL_set_error(env);
		return -1;
	}
	assert(ref.type == VALUE_REF);
	if( FIAL_copy_value(args[SET_ME], ref.ref, env->interp) ) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg =
			"Could not dupe contained  value -- bad alloc";
		FIAL_set_error(env);
		return -1;
	}
	return 0;
}


static int register_type (int argc, struct FIAL_value **args,
			  struct FIAL_exec_env *env,
			  void *ptr)
{
	int tmp, ret;
	if(argc == 0) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "register types requires an argument in which"
			                " to storet he new type";
		FIAL_set_error(env);
		return -1;
	}

	ret =  FIAL_register_type(&tmp, NULL, NULL, env->interp);
	if(ret == -1) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "out of memory while allocating space "
			                "to register type";
		FIAL_set_error(env);
		return -1;
	}
	if(ret == -2) {
		env->error.code = ERROR_INTERP_STATE;
		env->error.static_msg = "Attempt to register type when interpreter "
			                "Was not set to load state.";
		FIAL_set_error(env);
		return -1;
	}

	args[0]->type = VALUE_TYPE;
	args[0]->n    = tmp;
	return 0;
}



/* signals argument error if not enough errors are passed. */

static int get_type_of (int argc, struct FIAL_value **args,
		 struct FIAL_exec_env *env,
		 void *ptr)
{
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "must pass a value to typeof";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(args[0] && args[1]);

	FIAL_clear_value(args[0], env->interp);

	args[0]->type = VALUE_TYPE;
	args[0]->n    = args[1]->type;

	return 0;

}

static int map_create   (int argc, struct FIAL_value **args,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	if(argc < 1) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Argument to put new map in";
		FIAL_set_error(env);
		return -1;
	}
	assert(args);
	FIAL_clear_value(args[0], env->interp);
	args[0]->type = VALUE_MAP;
	args[0]->map = FIAL_create_symbol_map();
	if(!args[0]->map) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "Couldn't allocate map";
		FIAL_set_error(env);
		return -1;
	}
	return 0;
}

static int map_lookup   (int argc, struct FIAL_value **argv,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	int tmp;
	struct FIAL_value val;

	memset(&val, 0, sizeof(val));

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "not enough args to lookup";
		FIAL_set_error(env);
		return -1;
	}
	assert(argv);
	if(argv[1]->type != VALUE_MAP) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "arg2 must be a MAP.";
		FIAL_set_error(env);
		return -1;
	}
	if(argv[2]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "arg3 must be a symbol.";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_clear_value(argv[0], env->interp);
	tmp =  FIAL_map_lookup_and_clear(argv[0], argv[1]->map, argv[2]->sym,
					 env->interp);

	return 0;
}

static int map_dupe   (int argc, struct FIAL_value **argv,
		       struct FIAL_exec_env *env,
		       void *ptr)
{
	int tmp;
	struct FIAL_value val;

	memset(&val, 0, sizeof(val));

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "not enough args to lookup";
		FIAL_set_error(env);
		return -1;
	}
	assert(argv);
	if(argv[1]->type != VALUE_MAP) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "arg2 must be a MAP.";
		FIAL_set_error(env);
		return -1;
	}
	if(argv[2]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =  "arg3 must be a symbol.";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_clear_value(argv[0], env->interp);
	tmp =  FIAL_lookup_symbol(&val, argv[1]->map, argv[2]->sym);

	if(tmp == 0) {
		if (FIAL_copy_value(argv[0], &val, env->interp) < 0) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "couldn't dupe value from map.";
			FIAL_set_error(env);
			return -1;
		}
	}
	return 0;
}

static int map_set   (int argc, struct FIAL_value *argv[],
		      struct FIAL_exec_env *env,
		      void *ptr)
{
	int ret = 0;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "not enough args for map set";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(argv);
	if(argv[0]->type != VALUE_MAP) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "in map set, arg1 must be a MAP.";
		E_SET_ERROR(*env);
		return -1;
	}
	if(argv[1]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "in map set,arg2 must be a symbol.";
		E_SET_ERROR(*env);
		return -1;
	}
	ret =  FIAL_set_symbol(argv[0]->map, argv[1]->sym, argv[2], env);
	/* ok, I am deleting the value, since I can't allow copying of
	 * just anything here.  Not strictly necessary for all value,
	 * but then again, they should be easy to deal with using
	 * assignment.  */

	memset(argv[2], 0, sizeof(**(argv)));
	return ret;
}

static inline int is_primitive(struct FIAL_value *val)
{
	int ret;
	assert(val);
	switch(val->type) {
	case VALUE_NONE:
	case VALUE_ERROR:
	case VALUE_INT:
	case VALUE_FLOAT:
	case VALUE_SYMBOL:
	case VALUE_STRING:
	case VALUE_TYPE:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int global_set (int argc, struct FIAL_value **args,
		       struct FIAL_exec_env *env,
		       void *ptr)
{
	int tmp;

	assert(env);
	assert(env->lib);
	assert(env->interp);

	if(env->interp->state != FIAL_INTERP_STATE_LOAD) {
		env->error.code = ERROR_INTERP_STATE;
		env->error.static_msg = "Global values can only be set when "
			"interpreter is in a load state.";
		FIAL_set_error(env);
		return -1;
	}
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "arg error, global_set, requires 2 args";
		E_SET_ERROR(*env);
		return -1;

	}
	assert(args);
	if(args[0]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "set global: arg1 must be symbol.";
		E_SET_ERROR(*env);
		return -1;
	}
	if(!is_primitive(args[1])) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "global set arg2 only accepts primitive types!";
		E_SET_ERROR(*env);
		return -1;
	}
	if(!env->lib->global) {
		env->lib->global = FIAL_create_symbol_map();
		if(!env->lib->global) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "couldn't allocate a global symbol map!";
			E_SET_ERROR(*env);
			return -1;
		}
	}
	assert(env->lib->global);
	tmp =  FIAL_set_symbol(env->lib->global, args[0]->sym, args[1], env);

	if(tmp < 0) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "couldn't allocate a global symbol map!";
		E_SET_ERROR(*env);
		return -1;
	}

	return 0;
}

static int global_lookup (int argc, struct FIAL_value **args,
			  struct FIAL_exec_env *env,
			  void *ptr)
{
	assert(env);
	assert(env->lib);
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "global lookup requires 2 args";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(args);
	if(args[1]->type != VALUE_SYMBOL) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "global lookup : arg2 must be a symbol.";
		E_SET_ERROR(*env);
		return -1;
	}

	FIAL_clear_value(args[0], env->interp);

	if(!env->lib->global)
		return 1;

	assert(env->lib->global);

	return FIAL_lookup_symbol(args[0], env->lib->global, args[1]->sym);

}

#ifdef KEEP_ARRAYS

static int array_create (int argc, struct FIAL_value **args,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	if(argc < 1) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "need args! no place to put created array";
		E_SET_ERROR(*env);
		return -1;

	}
	assert(args);

	FIAL_clear_value(args[0], env->interp);
	args[0]->type   = VALUE_ARRAY;
	args[0]->array  = calloc(1, sizeof(struct FIAL_array));
	if(!args[0]->array) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "couldn't allocate space for array.";
		E_SET_ERROR(*env);
		return -1;
	}
	return 0;
}

static int array_size (int argc, struct FIAL_value **args,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "no arguments to array size -- need "
		                        "return value and array.";
		E_SET_ERROR(*env);
		return -1;
	}

	if(args[1]->type != VALUE_ARRAY) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "attempt to use array, size for something "
					"other than array";
		E_SET_ERROR(*env);
		return -1;
	}

	assert(args);

	FIAL_clear_value(args[0], env->interp);
	args[0]->type = VALUE_INT;
	args[0]->n    = args[1]->array->size;

	return 0;

}

static int array_get (int argc, struct FIAL_value **args,
		      struct FIAL_exec_env *env,
		      void *ptr)
{
	int tmp;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "not enough args to array_get";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(args);
	if(args[1]->type != VALUE_ARRAY) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "need array as arg2 to get value from.";
		E_SET_ERROR(*env);
		return -1;
	}
	if(args[2]->type != VALUE_INT || args[2]->n < 0) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "need non-negative int as arg3 to get from array.";
		E_SET_ERROR(*env);
		return -1;
	}
	tmp =  FIAL_array_get_index(args[0], args[1]->array, args[2]->n,
				    env->interp);
	assert(tmp >= 0);
	(void)tmp;
	return 0;
}

static int array_set (int argc, struct FIAL_value **args,
		      struct FIAL_exec_env *env,
		      void *ptr)
{
	int tmp;

	if(argc < 3) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "not enough args to array_set";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(args);
	if(args[0]->type != VALUE_ARRAY) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "need array to set value to";
		E_SET_ERROR(*env);
		return -1;
	}
	if(args[1]->type != VALUE_INT) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "need non negetive int to set array element.";
		E_SET_ERROR(*env);
		return -1;
	}
	tmp =  FIAL_array_set_index(args[0]->array, args[1]->n,args[2],
				    env->interp);
	if(tmp < 0) {
		assert( tmp != -2);
		assert( tmp == -1);

		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "coulnt' allocate space to set value in array.";
		E_SET_ERROR(*env);
		return -1;
	}
	return 0;
}
#endif /* KEEP_ARRAYS */

static int break_on_me (int argc, struct FIAL_value **args,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	return 0;
}

struct FIAL_c_func_def omni_lib[] = {
	{"print"    , print    ,      NULL},
	{"load"     , load_lib ,      NULL},
	{"move"     , move_value,     NULL},
	{"copy"     , copy_value,     NULL},
	{"register" , register_type , NULL},
	{"type_of"  , get_type_of ,   NULL},
	{"const"    , get_constant,   NULL},
	{"break"    , break_on_me,    NULL},
	{"take"     , omni_take   ,   NULL},
	{"put"      , omni_put    ,   NULL},
	{"dupe"     , omni_dupe   ,   NULL},
	{NULL       , NULL        ,   NULL}
};

struct FIAL_c_func_def map_lib[] = {
	{"take"  , map_lookup  ,   NULL},
	{"put"   , map_set     ,   NULL},
	{"create", map_create  ,   NULL},
	{"dupe"  , map_dupe    ,   NULL},
	{NULL    , NULL        ,   NULL}
};
#ifdef KEEP_ARRAYS
struct FIAL_c_func_def array_lib[] = {
	{"create", array_create ,   NULL},
	{"set"   , array_set    ,   NULL},
	{"get"   , array_get    ,   NULL},
	{"size"  , array_size   ,   NULL},
	{NULL    , NULL         ,   NULL}
};
#endif /* KEEP_ARRAYS */

struct FIAL_c_func_def global_lib[] = {
	{"set"   , global_set    ,   NULL},
	{"lookup", global_lookup ,   NULL},
	{NULL    , NULL          ,   NULL}
};

int FIAL_install_std_omnis (struct FIAL_interpreter *interp)
{
	struct FIAL_c_lib *omni_c_lib = NULL;
	struct FIAL_c_lib *lib = NULL;
	struct FIAL_c_lib *map_l_lib = NULL;
	FIAL_symbol sym;

	FIAL_load_c_lib(interp, "std_procs", omni_lib  , &omni_c_lib);
	FIAL_get_symbol(&sym,   "omni"     , interp);
	assert(sym);
	FIAL_add_omni_lib(interp, sym, omni_c_lib);

	FIAL_load_c_lib(interp, "map", map_lib, &map_l_lib);
	FIAL_get_symbol(&sym,   "map"     , interp);
	assert(sym);
	FIAL_add_omni_lib(interp, sym, map_l_lib);

	sym = 0;
	FIAL_load_c_lib(interp, "global", global_lib, &lib);
	FIAL_get_symbol(&sym,   "global", interp);
	assert(sym);
	FIAL_add_omni_lib(interp, sym, lib);


	return 0;
}
