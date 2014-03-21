#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "interp.h"
#include "api.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "ast.h"
#include "error_def_short.h"
#include "error_macros.h"
#include "text_buf.h"
#include "loader.h"


/* another pita -- this will have to get rewritten somewhat with the
 * new sequence data structure.  */


static int get_filenames (struct FIAL_array *ar,  char *dir_name,
			  int len, struct FIAL_interpreter *interp)
{
	struct FIAL_value val;
	int i;
	struct dirent *dp;
	DIR *dfd;


	memset(&val, 0, sizeof(val));
	i = 0;
	dp = NULL;
	dfd = opendir(".");

	if(!dfd) {
		return 1;
	}
	while((dp = readdir(dfd)) != NULL) {
		struct stat s;
		struct FIAL_text_buf *tb = FIAL_create_text_buf();

		if(!tb) {

			/* should I clean this up, or just return the
			 * partial?  I'm just returning the partial
			 * now. */

			return -1;
		}

		/* skip directories, not sure how to handle this in
		 * the general case.  I'm justing doing what I need
		 * right now I guess, I will just improve it as I go
		 * along....
		 */
		if(stat(dp->d_name, &s) || S_ISDIR(s.st_mode))
			continue;

		if(FIAL_text_buf_append_str(tb, dp->d_name)<0) {
			FIAL_destroy_text_buf(tb);
			return -2;
		}

		val.type = VALUE_TEXT_BUF;
		val.text = tb;

		FIAL_array_set_index(ar, ++i, &val, interp);

		assert(val.type == VALUE_NONE);
		assert(val.n    == 0);

	}

	/*FIXME: error checking..... */

	return 0;

}

/* oh god this is aweful, but I'm just cutting and pasting this entire
   function from the WINDOWS thing.  Obviously, what I need is a
   sys_common.c, a sys_bsd.c, and a sys_win.c, but the problem is
   these functions are declared as static, so they have to be in the
   same source file.  Obviously nothing that can't be worked around,
   and that this is unacceptable as a long term solution, but for now
   it is pretty easy. */

static int get_directory_filenames (int argc, struct FIAL_value **argv,
				    struct FIAL_exec_env *env, void *ptr)
{
	char *str = NULL;
	int ret;
	int len;

	if(argc < 2 ||
	   !(argv[1]->type == VALUE_STRING || argv[1]->type == VALUE_TEXT_BUF)) {

		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Getting filenames from directory "
			"requires a value to put the array of names in, and "
			"a directory name.";
		E_SET_ERROR(*env);
		return -1;
	}
	if(argv[1]->type == VALUE_STRING)  {
		str = argv[1]->str;
		assert(str);
		len = strlen(str);

	} else {
		assert(argv[1]->type == VALUE_TEXT_BUF);
		assert(argv[1]->text);
		if(!argv[1]->text->buf) {
		}
		str = argv[1]->text->buf;
		len = argv[1]->text->len;
	}
	if(!str || !len) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "empty path passed to get directory filenames";
		E_SET_ERROR(*env);
		return -1;
	}

	assert(str);
	assert(len);

	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_ARRAY;
	argv[0]->array = calloc(1, sizeof(*argv[0]->array));
	if(!argv[0]->array) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "Get directory filenames couldn't "
			                "allocate array.";
		E_SET_ERROR(*env);
		return -1;
	}
	ret = get_filenames(argv[0]->array, str, len, env->interp);
	if(ret < 0) {
		if(ret == -1) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg="Get directory filenames ran "
			    "out of mem.";
			E_SET_ERROR(*env);
		} else {
			assert(ret == -2);
			env->error.code = ERROR_RUNTIME;
			env->error.static_msg="Error retrieving filenames "
			    "from directory";
			E_SET_ERROR(*env);
		}
	}
	return ret;
}

/* ditto for this function, cut and paste with a one character change...... */

static int get_pid (int argc, struct FIAL_value **args,
		    struct FIAL_exec_env *env, void *ptr)
{
	pid_t pid = getpid();  /* i don't know how else to do this,
				 * GetCurrentProcessId appears to be
				 * C++ only...*/

	if(argc > 0) {
		FIAL_clear_value (args[0], env->interp);
		args[0]->type = VALUE_INT;
		args[0]->n    = pid;
		return 0;
	}
	return 1;
}

static int check_if_dir (char *path)
{
	struct stat st;
	if(!stat(path, &st))
		return 0;
	if(S_ISDIR(st.st_mode))
		return 1;
	else
		return 0;
}

/* more cut and paste */
static int is_dir (int argc, struct FIAL_value **argv,
		   struct FIAL_exec_env *env, void *ptr)
{
	char *str;

	if(argc < 2 ||
	   (argv[1]->type != VALUE_STRING && argv[1]->type != VALUE_TEXT_BUF)) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Need return value and filename to check whether it is "
			"a dir.";
		FIAL_set_error(env);
		return -1;
	}
	if (argv[1]->type == VALUE_STRING) {
		str = argv[1]->str;
	} else {
		assert(argv[1]->type == VALUE_TEXT_BUF);
		str = argv[1]->text->buf;
	}
	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_INT;
	argv[0]->n    = check_if_dir(str);
	return 0;
}

static int get_env (int argc, struct FIAL_value **argv,
		    struct FIAL_exec_env *env, void *ptr)
{
	char *str, *var;

	if(argc < 2 || (argv[1]->type != VALUE_STRING &&
			argv[1]->type != VALUE_TEXT_BUF  ))
		return 1; /* why isn't this an error? I don't feel like typing. */
	assert(argv[0]);
	if(argv[1]->type == VALUE_STRING) {
		var = argv[1]->str;
	} else {
		assert(argv[1]->type == VALUE_TEXT_BUF);
		var = argv[1]->text->buf;
	}

	FIAL_clear_value (argv[0], env->interp);

	if(!(str = getenv(var)))
		return 1;

	argv[0]->type = VALUE_TEXT_BUF;
	argv[0]->text = FIAL_create_text_buf();

	if(!argv[0]->text) {
		argv[0]->type = 0;
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg =
			"couldn't allocate buffer for env. variable.";
		FIAL_set_error(env);
		return -1;
	}

	FIAL_text_buf_append_str(argv[0]->text, str);
	return 0;
}

/* this is again all cut and paste.  I think it will be best to use
 * one source file, this is ridiculous.... */

static char *get_str (struct FIAL_value *val)
{
	char *str;
	switch(val->type) {
	case VALUE_STRING:
		str = val->str;
		break;
	case VALUE_TEXT_BUF:
		if(!val->text){
			str = NULL;
			break;
		}
		str = val->text->buf;
		break;
	default:
		str = NULL;
		break;
	}
	return str;
}


static int system_run_cmd (int argc, struct FIAL_value **a,
		    struct FIAL_exec_env *env, void *ptr)
{
	char *str;
	enum{CMD = 0};
	if(argc < 1)
		return 1;
	str = get_str(CMD[a]);
	if(!str)
		return 1;
	system(str);
	return 0;
}

int FIAL_install_system_lib (struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def lib_system[] = {
		{"read_dir"  , get_directory_filenames , NULL},
		{"get_pid"   , get_pid                 , NULL},
		{"is_dir"    , is_dir                  , NULL},
		{"get_env"   , get_env                 , NULL},
		{"run"       , system_run_cmd          , NULL},
		{NULL, NULL, NULL}
	};

	return FIAL_load_c_lib (interp, "system", lib_system , NULL);
}
