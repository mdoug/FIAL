/* 
 * Ok, the point here is to use the FIAL_proc struct as a value.  
 * 
 * Also, struct FIAL_error_info.  
 * 
 * For now, this will just have the stuff to execute in its own exec
 * environment.  
 *
 * But, this will also be the basis for threading, though I suppose that
 * can come a bit later.  
 */

/* 
 * Sigh.  My proc usage is a little messed up.  It uses a union lib_entry *,
 * but that pointer is pointing to something that _isn't_ necessasarily a
 * union, but is just one of the members.  I am not entirely sure this is
 * legal, but it definitely requires care to prevent segfaulting.  I.e., you
 * can't copy it, without risking segfaulting (you can only copy the
 * appropriate member.)
 */

#include <Windows.h>

#include <assert.h>
#include "FIAL.h"

static int get_lib (union FIAL_lib_entry **lib, 
                    struct FIAL_value *val,
                    struct FIAL_exec_env *env)
{
	char *str;
	if (val->type == VALUE_SYMBOL) {
		struct FIAL_value tmp;
		if (!env || !env->lib ||
		    FIAL_lookup_symbol(&tmp, env->lib->libs, val->sym) != 0)  
			return -1;
		assert (tmp.type == VALUE_LIB || tmp.type == VALUE_C_LIB);
		if (tmp.type == VALUE_LIB) 
			*lib = (union FIAL_lib_entry *)tmp.lib;
		else 
			*lib = (union FIAL_lib_entry *)tmp.c_lib;
		return 0;
	}
	assert (val->type == VALUE_STRING || val->type == VALUE_TEXT_BUF);
	str = FIAL_get_str (val);
	if (!FIAL_load_lookup(env->interp, str, lib))
		return -1;
	return 0;
}

static int proc_create (int argc, struct FIAL_value **argv,
                        struct FIAL_exec_env *env, void *ptr)
{
	char *str;
	struct FIAL_proc *proc;
	FIAL_symbol proc_sym;

	assert(env);
	(void) ptr;
	if((argc < 3 ||
	    (argv[1]->type != VALUE_SYMBOL && 
	     argv[1]->type != VALUE_STRING &&
	     argv[1]->type != VALUE_TEXT_BUF) ||
	    argv[2]->type != VALUE_SYMBOL) &&
	   (argc != 2 ||
	    argv[1]->type != VALUE_SYMBOL)) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = 
		"Need (1) value to place new proc into, (2) string containing"
		"lib label,\n"
		"or symbol denoting library, and (3) symbol of proc.\n"
		"If two procs are given, current library will be used.";
		FIAL_set_error (env);
		return -1;
	}
	proc = calloc(sizeof(*proc), 1);
	if (!proc) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "Couldn't allocate procedure.";
		FIAL_set_error(env);
		return -1;
	}
	proc->interp = env->interp;
	if (argc == 2) {
		assert(argv[1]->type == VALUE_SYMBOL);
		proc->lib = (union FIAL_lib_entry *)(env->lib);
		proc_sym = argv[1]->sym;
	} else {
		if (get_lib (&proc->lib, argv[1], env) < 0) {
			free(proc);
			env->error.code = ERROR_RUNTIME;
			env->error.static_msg = "Unknown lib";
			FIAL_set_error(env);
			return -1;
		}
		assert(argv[2]->type == VALUE_SYMBOL);
		proc_sym = argv[2]->sym;
	}
	if (proc->lib->type == FIAL_LIB_FIAL) {
		if (FIAL_lookup_symbol(&proc->proc, proc->lib->lib.procs,
		                       proc_sym) != 0) {
			free(proc);
			env->error.code = ERROR_RUNTIME;
			env->error.static_msg = "Unboud procedure";
			FIAL_set_error (env);
			return -1;
		}
	} else {
		assert(proc->lib->type == FIAL_LIB_C);
		if(FIAL_lookup_symbol(&proc->proc, proc->lib->c_lib.funcs,
					argv[2]->sym) != 0) {
			free(proc);
			env->error.code = ERROR_RUNTIME;
			env->error.static_msg = "Unboud procedure";
			FIAL_set_error (env);
			return -1;
		}
	}
	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_PROC;
	argv[0]->proc = proc;
	return 0;
}

static int proc_run (int argc, struct FIAL_value **argv,
                     struct FIAL_exec_env *env, void *ptr)
{
	int i;
	struct FIAL_value *arg_strip = NULL;
	struct FIAL_exec_env tmp;
	int ret;

	(void) ptr;
	if (argc < 2 ||
	    argv[1]->type != VALUE_PROC) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = 
		"Need (1) return value for possible error, (2) proc, to run "
		"a proc";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_init_exec_env (&tmp);
	if(argc > 2) {
		/* FIXME: alloca or variable length array should be
                          considered here.  */
		arg_strip = malloc (sizeof(*arg_strip) * (argc-2+1));
		if (!arg_strip) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = 
			"Couldn't allocate space for arguments";
			FIAL_set_error(env);
			FIAL_deinit_exec_env(&tmp);
			return -1;
		}
		for (i = 0; i < (argc-2); i++)
			arg_strip[i] = *argv[i+2];
	}
	ret = FIAL_run_proc(argv[1]->proc, arg_strip, &tmp);
	if(arg_strip) {
		for (i = 0; i < (argc-2); i++)
			*argv[i+2] = arg_strip[i];
		arg_strip[i].type = VALUE_END_ARGS;
		free(arg_strip);
	}
	if (ret < 0) {
		struct FIAL_error_info *err = malloc(sizeof(*err));
		if(!err) {
			tmp.error.code = ERROR_BAD_ALLOC;
			tmp.error.static_msg = 
			"Couldn't allocate space for error info";
			FIAL_set_error(env);
		}
		memcpy(err, &tmp.error, sizeof(*err));
		FIAL_clear_value(argv[0], env->interp);
		argv[0]->type = VALUE_ERROR_INFO;
		argv[0]->err = err;
		FIAL_deinit_exec_env(&tmp);
		return 0;

	}
	FIAL_deinit_exec_env(&tmp);
	return 0;
}

/* ok, I will work on refactoring this stuff to share code later.  For now,
 * I will just do a crude rewrite of the run code, only launching a thread */

/* here I need some basic threading in order to launch a proc if I need 
   to, but first I'll get the run stuff going */

struct launcher_args {
	struct FIAL_proc proc;
	struct FIAL_value *args;
};

static DWORD WINAPI launcher (void *ptr)
{
	int ret;
	struct launcher_args *la = ptr;
	struct FIAL_exec_env env;
	struct FIAL_value *iter;

	if (FIAL_init_exec_env(&env) < 0) {
		ret = -1;
		goto cleanup;
	}
	ret = FIAL_run_proc(&la->proc, la->args, &env);
	FIAL_deinit_exec_env(&env);
	
cleanup:
	if (la->args) 
		for (iter = la->args; iter->type != VALUE_END_ARGS; iter++)
			FIAL_clear_value(iter, la->proc.interp);
	free(la->args);
	free(la);
	return ret;
}

static int proc_launch (int argc, struct FIAL_value **argv, 
                        struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_value *arg_strip;
	struct launcher_args *la = calloc(sizeof(*la), 1);
	HANDLE thread;
	int i;

	if (argc < 1 ||
	    argv[0]->type != VALUE_PROC) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = 
		"Need (1) return value for possible error, (2) proc, to launch "
		"a proc";
		FIAL_set_error(env);
		return -1;
	}
	if(!la) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = 
			"Couldn't allocate space for arguments";
			FIAL_set_error(env);
			return -1;
	}
	if(argc > 1) {
		arg_strip = calloc (sizeof(*arg_strip), argc);
		if (!arg_strip) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = 
			"Couldn't allocate space for arguments";
			FIAL_set_error(env);
			return -1;
		}
		for (i = 0; i < (argc-1); i++)
			FIAL_move_value(arg_strip + i, argv[i+1], env->interp);
		arg_strip[i].type = VALUE_END_ARGS;
	}
	la->proc = *argv[0]->proc;
	la->args = arg_strip;
	
	thread = CreateThread(NULL, 0, launcher, la, 0, NULL);
	CloseHandle(thread);
	return 0;
}

static int error_static_msg (int argc, struct FIAL_value **argv,
                             struct FIAL_exec_env *env, void *ptr)
{
	const char *str;

	(void)ptr;
	if (argc < 2 ||
	    argv[1]->type != VALUE_ERROR_INFO ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = 
		"Need (1) return value, (2) error info, to get static msg"
		" from error info.";
		FIAL_set_error(env);
		return -1;
	}
	str = argv[1]->err->static_msg;
	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_STRING;
	argv[0]->str  = str;
	return 0;
}

static int proc_finalizer (struct FIAL_value *val,
                           struct FIAL_interpreter *interp,
                           void *ptr)
{
	free (val->proc);
	memset (val, 0, sizeof(*val));
	return 0;
}

static int err_info_finalizer (struct FIAL_value *val,
                               struct FIAL_interpreter *interp,
                               void *ptr)
{
	free (val->err->dyn_msg);
	free(val->err);
	memset(val, 0, sizeof(*val));
	return 0;
}

static int err_info_copier (struct FIAL_value *to,
                            struct FIAL_value *from,
                            struct FIAL_interpreter *interp,
                            void *ptr)
{
	if (to == from)
		return 0;
	FIAL_clear_value(to, interp);
	to->type = VALUE_ERROR_INFO;
	to->err = malloc(sizeof(*to->err));
	if(!to->err)
		return -1;
	memcpy(to->err, from->err, sizeof(*to->err));
	if(from->err->dyn_msg) {
		int len = strlen(from->err->dyn_msg);
		to->err->dyn_msg = malloc(len + 1);
		if(!to->err->dyn_msg) {
			free(to->err);
			return -1;
		}
		memcpy(to->err->dyn_msg, from->err->dyn_msg, len+1);
	}
	return 0;
}

int FIAL_install_proc (struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def lib_proc[] = {
		{"create"    , proc_create      , NULL},
		{"run"       , proc_run         , NULL},
		{"launch"    , proc_launch      , NULL},
		{"static_msg", error_static_msg , NULL},
		{NULL        , NULL             , NULL}
	};
	struct FIAL_finalizer proc_fin = {proc_finalizer, NULL};
	struct FIAL_finalizer err_fin = {err_info_finalizer, NULL};
	struct FIAL_copier err_cpy = {err_info_copier, NULL};

	interp->types.finalizers[VALUE_PROC] = proc_fin;
	interp->types.finalizers[VALUE_ERROR_INFO] = err_fin;
	interp->types.copiers[VALUE_ERROR_INFO]=err_cpy;

	if(FIAL_load_c_lib (interp, "proc", lib_proc, NULL) < 0) 
		return -1;
	return 0;
}
