 /**********************************************************************
 *								       *
 *  This file contains stuff for the system library. 		       *
 *								       *
 *  When I port to the first system other than windows I will worry    *
 *  about separating out the stuff for different systems.  	       *
 * 								       *
 *  I think it would appear to be simplest, to rewrite this file for   *
 *  FreeBSD, the #ifdefs would be all pervasive otherwise. 	       *
 * 								       *
 *								       *
 *  man, I don't know what I'm doing, this Windows unicode stuff is so *
 *  confusing.  I am just going to pretend it doesn't exist until I am *
 *  swarmed by a million trillion error messages.		       *
 *								       *
 **********************************************************************/

#include <windows.h>
#include <process.h>   /* need _getpid ()  */
#include <stdlib.h>
#include <stdio.h>

#include "interp.h"
#include "api.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "ast.h"
#include "error_def_short.h"
#include "error_macros.h"
#include "text_buf.h"
#include "loader.h"

/*
 *  Functions for calling from C.
 */


/*
 *  Functions for calling from FIAL
 */

/*
 *  This will return all the filenames in a directory, in an array of
 *  text_bufs.  Note to self -- make some text_bufs.
 */

/*
  return --

  -1:
  bad alloc

  -2:
  path too long.

*/

static int get_filenames (struct FIAL_array *ar,  char *dir_name,
			  int len, struct FIAL_interpreter *interp)
{
	char buf[MAX_PATH];
	WIN32_FIND_DATA find_data;
	HANDLE find_handle = INVALID_HANDLE_VALUE;
	struct FIAL_value val;

	int i = 0;
	memset(&val, 0, sizeof(val));

	if (len == 0)
		len = strlen(dir_name);

	if(len + 3 > MAX_PATH)
		return -2;

	strcpy(buf, dir_name);
	strcpy(buf + len, "\\*");

	find_handle = FindFirstFile(buf, &find_data);
	if(find_handle == INVALID_HANDLE_VALUE) {
		return 1;
	}
	do {
		int tmp;
		struct FIAL_text_buf *tb = FIAL_create_text_buf();

		if(!tb) {
			return -1;
		}

		/* skip directories, not sure how to handle this in
		 * the general case.  I'm justing doing what I need
		 * right now I guess, I will just improve it as I go
		 * along....
		 */

		if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		if((tmp = FIAL_text_buf_append_str(tb, find_data.cFileName)<0)){
			FIAL_destroy_text_buf(tb);
			return tmp;
		}

		val.type = VALUE_TEXT_BUF;
		val.text = tb;

		FIAL_array_set_index(ar, ++i, &val, interp);

		assert(val.type == VALUE_NONE);
		assert(val.n    == 0);

	} while (FindNextFile(find_handle, &find_data) != 0);

	/*FIXME: error checking..... */

	return 0;

}

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

static int get_pid (int argc, struct FIAL_value **args,
		    struct FIAL_exec_env *env, void *ptr)
{
	DWORD pid = _getpid();  /* i don't know how else to do this,
				 * GetCurrentProcessId appears to be
				 * C++ only...*/

	assert(sizeof (DWORD) == sizeof(int));

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
	WIN32_FIND_DATA find_data;
	HANDLE find_handle = INVALID_HANDLE_VALUE;

	find_handle = FindFirstFile(path, &find_data);
	if(find_handle == INVALID_HANDLE_VALUE) {
		return 0;
	}
	if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return 1;
	return 0;
}

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

/* yeah, this is definitely a special purpose function at this point,
 * I should move it to my comic viewer.  */

static int create_process_run_cmd(int argc, struct FIAL_value **argv,
				  struct FIAL_exec_env *env, void *ptr)
{
	char *str;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	int ret;

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &pi, sizeof(pi) );

	if(argc < 1 || !(argv[0]->type == VALUE_STRING ||
			 argv[0]->type == VALUE_TEXT_BUF)) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Must pass string or text buf"
			"to run cmd.";
		FIAL_set_error(env);
		return -1;
	}


	str = get_str(argv[0]);
	// Start the child process.
	if( !CreateProcess( NULL,           // No module name (use command line)
			    str,            // Command line
			    NULL,           // Process handle not inheritable
			    NULL,           // Thread handle not inheritable
			    FALSE,          // Set handle inheritance to FALSE
			    CREATE_NO_WINDOW,              // No creation flags
			    NULL,           // Use parent's environment block
			    NULL,           // Use parent's starting directory
			    &si,            // Pointer to STARTUPINFO structure
			    &pi )           // Pointer to PROCESS_INFORMATION structure
		)
		ret = 1;
	else
		ret = 0;

	if(pi.hProcess != NULL)
		WaitForSingleObject(pi.hProcess, INFINITE);

	return ret;
}



int (*run_cmd) (int argc, struct FIAL_value **, struct FIAL_exec_env *, void *)=
#ifdef USE_SYSTEM_RUN_CMD
    system_run_cmd;
#else
create_process_run_cmd;
#endif /*USE_SYSTEM_RUN_CMD*/

int FIAL_install_system_lib (struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def lib_system[] = {
		{"read_dir"  , get_directory_filenames , NULL},
		{"get_pid"   , get_pid                 , NULL},
		{"is_dir"    , is_dir                  , NULL},
		{"get_env"   , get_env                 , NULL},
		{"run"       , run_cmd                 , NULL},
		{NULL, NULL, NULL}
	};

	return FIAL_load_c_lib (interp, "system", lib_system , NULL);
}
