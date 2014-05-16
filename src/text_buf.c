#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "interp.h"
#include "api.h"  /* needed for FIAL_clear_value */
#include "ast.h"  /*needed for errors.....*/
#include "error_def_short.h"
#include "error_macros.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "loader.h"

/* this is for text buffer class!  it holds text.  This is not really
   a string processing language, so I don't have to go too overboard.
   The main this is that I need some form of dynamically allocated
   string support.  */

/* for starters I am going to try to not initialize memory, since
   doing so will be awfully slow for some operations.  Instead I will
   try to just make sure everything has a trailing '\0'.  If this gets
   buggy, I will just initialize to zero and be done with it until I
   really want to figure this out better. */

struct FIAL_text_buf  *FIAL_create_text_buf (void)
{
	struct FIAL_text_buf *buf = malloc(sizeof(*buf));
	if(!buf)
		return NULL;

	memset(buf, 0, sizeof(*buf));
	return buf;
}

void FIAL_destroy_text_buf (struct FIAL_text_buf *buffy)
{
	if(buffy) {
		free(buffy->buf);
		free(buffy);
	}
}

static inline size_t size_t_max (size_t a, size_t b, size_t c)
{
	if (a > b && a > c)
		return a;
	return b > c ? b : c;
}

#define DEFAULT_BUF_START_SIZE 32

int FIAL_text_buf_reserve (struct FIAL_text_buf *buffy, size_t space)
{
	if(!buffy)
		return -1;
	if((buffy->cap == buffy->len) ||
	   ((buffy->cap - buffy->len - 1) < space)) {
		size_t new_size;
		char *tmp;

		assert(!((buffy->cap == buffy->len) && buffy->cap != 0));
		new_size = size_t_max(DEFAULT_BUF_START_SIZE,
				      buffy->cap + space + 1,
				      buffy->cap * 2);
		tmp = realloc(buffy->buf, new_size);
		if(!tmp)
			return -1;
		buffy->buf = tmp;
		buffy->cap = new_size;
	}
	return 0;
}

/* note: *str should not overlap *buffy->buf, memmove is used anyway. */

int FIAL_text_buf_append_str (struct FIAL_text_buf *buffy, char *str)
{
	int len = strlen(str);
	if(FIAL_text_buf_reserve(buffy, len) == -1)
		return -1;
	assert(buffy->cap >= len + 1);
	assert(buffy->cap >= len + 1+ buffy->len);
	memmove(buffy->buf + buffy->len, str, len + 1);
	buffy->len += len;
	return 0;
}

int FIAL_text_buf_append_text_buf (struct FIAL_text_buf *buffy,
				   struct FIAL_text_buf *willow)
{
	if(willow->buf == NULL)
		return 0;
	if(FIAL_text_buf_reserve(buffy, willow->len) == -1)
		return -1;
	memmove(buffy->buf + buffy->len, willow->buf, willow->len + 1);
	buffy->len += willow->len;
	return 0;
}

int FIAL_text_buf_append_value (struct FIAL_text_buf *buffy,
				struct FIAL_value     *val)
{
	int ret = -2;
	char buf[256];
	switch (val->type) {
	case VALUE_STRING:
		ret = FIAL_text_buf_append_str(buffy, val->str);
		break;
	case VALUE_INT:
		sprintf(buf, "%d", val->n);
		ret = FIAL_text_buf_append_str(buffy, buf);
		break;
	case VALUE_FLOAT:
		sprintf(buf, "%f", val->x);
	        ret = FIAL_text_buf_append_str(buffy, buf);
		break;
	case VALUE_TEXT_BUF:
		ret = FIAL_text_buf_append_text_buf(buffy, val->text);
		break;
	default:
		sprintf(buf, "(value of type: %d)", val->type);
		ret = FIAL_text_buf_append_str(buffy, buf);
		if(ret == 0)
			ret = 1;
		break;
	}
	return ret;

}

/***********************************************************************
 *								       *
 * FIAL functions for the "text_buf" library!.  This should be good.   *
 *								       *
 ***********************************************************************/

static int text_buf_create (int argc, struct FIAL_value **argv,
			    struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_text_buf *buffy  = NULL;

	if(argc < 1) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Argument to put new text buffer in "
			"required.";
		FIAL_set_error(env);
		return -1;
	}

	assert(argv);
	FIAL_clear_value(argv[0], env->interp);
	buffy = FIAL_create_text_buf();
	if(!buffy) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "unable to allocate text buffer.";
		FIAL_set_error(env);
		return -1;
	}

	argv[0]->type = VALUE_TEXT_BUF;
	argv[0]->text = buffy;

	return 0;
}

static int text_buf_append(int argc, struct FIAL_value **argv,
			   struct FIAL_exec_env *env, void *ptr)
{
	int i;
	int ret;

	if(argc < 2 || argv[0]->type != VALUE_TEXT_BUF) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Arguments to text buf append are a text"
			" buf and a value to append.";
		E_SET_ERROR(*env);
		return -1;
	}
	assert(argv[0]->text);
	for(i = 1; i < argc; i++) {
		ret = FIAL_text_buf_append_value(argv[0]->text, argv[i]);
		if (ret < 0) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg =
				"Couldn't append value to text buf.";
			E_SET_ERROR(*env);
			break;
		}
	}
	return ret;
}

static int text_buf_compare(int argc, struct FIAL_value **argv,
			    struct FIAL_exec_env *env, void *ptr)
{
	char *str[2];
	int i;

	if(argc < 3 ||
	   (argv[1]->type != VALUE_STRING && argv[1]->type != VALUE_TEXT_BUF) ||
	   (argv[2]->type != VALUE_STRING && argv[2]->type != VALUE_TEXT_BUF)) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Arguments to text buf append are a "
		       "return arg, followed by two string or text buf values.";
		E_SET_ERROR(*env);
		return -1;
	}

	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_INT;
	for(i = 0; i < 2; i++) {
		switch(argv[i + 1]->type) {
		case VALUE_STRING:
			str[i] = argv[i+1]->str;
			break;
		case VALUE_TEXT_BUF:
			str[i] = argv[i+1]->text->buf;
			break;
		default:
			assert(0);
			break;
		}
	}

	if(!str[0] && !str[1])
		argv[0]->n = 1;
	else if(!str[0] || !str[0])
		argv[0]->n = 0;
	else
		argv[0]->n  = strcmp(str[0], str[1]) ? 1 : 0;

	return 0;
}

static int text_buf_finalize (struct FIAL_value *val,
			      struct FIAL_interpreter *interp,
			      void *ptr)
{
	assert(val);
	assert(val->type = VALUE_TEXT_BUF);
	FIAL_destroy_text_buf(val->text);

	return 0;
}

static int text_buf_copier (struct FIAL_value *to,
			    struct FIAL_value *from,
			    struct FIAL_interpreter *interp,
			    void *ptr)
{
	assert(from);
	assert(from->type == VALUE_TEXT_BUF);
	if(to == from)
		return 0;
	FIAL_clear_value(to, interp);

	to->type = VALUE_TEXT_BUF;
	to->text  = FIAL_create_text_buf();
	if(!to->text)
		return -1;
	return FIAL_text_buf_append_text_buf(to->text, from->text);
}

int FIAL_install_text_buf (struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def lib_text_buf[] =
		{
			{"create"  , text_buf_create , NULL},
			{"append"  , text_buf_append , NULL},
			{"compare" , text_buf_compare, NULL},
			{NULL, NULL, NULL}
		};

	struct FIAL_finalizer fin = {text_buf_finalize, NULL};
	struct FIAL_copier    cpy = {text_buf_copier,   NULL};

	if( FIAL_load_c_lib (interp, "text_buf", lib_text_buf, NULL) < 0)
		return -1;

	interp->types.finalizers[VALUE_TEXT_BUF] = fin;
	interp->types.copiers[VALUE_TEXT_BUF] = cpy;

	return 0;
}
