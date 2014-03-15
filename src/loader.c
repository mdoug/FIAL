#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "loader.h"
#include "interp.h"
#include "ast.h"
#include "ast_def_short.h"
#include "basic_types.h"
#include "value_def_short.h"
#include "error_def_short.h"
#include "parser_defines.h"

/*must generate this file with flex on lexer.l.  Maybe should define
 * the name by the build system.  */

#include "flex_header.h"

typedef struct FIAL_interpreter       interpreter;
typedef struct FIAL_ast_node          node;
typedef struct FIAL_library           library;
typedef struct FIAL_symbol_map        symbol_map;
typedef struct FIAL_symbol_map_entry  entry;
typedef FIAL_symbol                   symbol;
typedef struct FIAL_value             value;

#define ALLOC(x)         malloc(x)
#define STRING_ALLOC(x)  malloc(x)

extern node *top;
extern interpreter *current_interp;

/*this is a little better than just asserting for errors, since now I
  at least can search through later for LOAD_ERROR.  Main problem with
  just asserts as errors is that it isn't clear whats an assertion and
  what is an error. */

/*#define LOAD_ERROR(x)         assert((x, 0))
#define ALLOC_ERROR(x)        assert((x, 0))
#define ERROR(x)              assert((x, 0))*/
/*#define set_symbol(x,y,z,w)   FIAL_set_symbol(x,y,z,w)*/

/*note: this is not a good interface.  Good enough for a static
 * function though.  ret_lib can be NULL if you don't want the address
 * of the new library.  */

/*

   returns:

   -1 for bad alloc
   -2 for duplicate procs

*/

static inline int add_lib_from_ast(interpreter             *interp,
				   node                    *top_node,
				   const char             *lib_label,
				   union FIAL_lib_entry   **ret_lib,
				   struct FIAL_error_info  *error)

{
	union FIAL_lib_entry *lib_entry = ALLOC(sizeof(*lib_entry));
	library *lib = NULL;

	node *iter = NULL;
	value val;
	symbol_map *m = NULL;
	char *new_label = NULL;

	if(ret_lib)
		*ret_lib = lib_entry;

	if(!lib_entry) {
		error->code = ERROR_BAD_ALLOC;
		error->static_msg = "couldn't allocate for lib entry when loading";
		return -1;
	};
	lib = &lib_entry->lib;

	memset(lib, 0, sizeof(*lib));
	memset(&val, 0, sizeof(val));

	m = FIAL_create_symbol_map();

	for(iter = top_node->left; iter; iter = iter->right) {
		int tmp;
		if( (tmp = FIAL_lookup_symbol(&val, m, iter->sym)) == 0) {
			error->code = ERROR_DUPLICATE_PROC;
			error->static_msg = "two procs with same name.";
			return -1;
		} else  {
			assert(tmp == 1);
			entry *ent = ALLOC(sizeof(*ent));
			if(!ent) {
				error->code = ERROR_BAD_ALLOC;
				error->static_msg = "couldn't allocate for proc "
					            "entry when loading";
				return -1;
			}

			memset(ent, 0, sizeof(*ent));
			ent->sym       = iter->left->sym;
			ent->val.type  = VALUE_NODE;
			ent->val.node  = iter->left;
			ent->next      = m->first;
			m->first       = ent;
		}
	}

	lib->type    = FIAL_LIB_FIAL;
	lib->procs   = m;
	lib->next    = interp->libs;
	interp->libs = lib_entry;

	assert(lib_label);
	new_label = STRING_ALLOC(strlen(lib_label) + 1);
	if(!new_label) {
		error->code = ERROR_BAD_ALLOC;
		error->static_msg = "couldn't allocate for lib_label";
		return -1;
	}

	strcpy(new_label, lib_label);
	lib->label = new_label;

	return 0;
}

int FIAL_load_file(struct FIAL_interpreter *interp,
		   const char             *filename,
		   union FIAL_lib_entry   **ret_lib,
		   struct FIAL_error_info    *error)

{
	extern struct FIAL_error_info *FIAL_parser_error_info;

	extern struct FIAL_ltype yylloc;
	extern int FIAL_parser_line_no;
	extern int FIAL_parser_col_no;

	union FIAL_lib_entry *lib_ent = NULL;

	FILE* input_file = NULL;
	library *new_lib = NULL;
	symbol sym;
	int tmp;

	int yyparse(void);

	error->file = (char *)filename;

	errno = 0;
/*	printf("filename to open :%s\n", filename);*/
	input_file = fopen(filename, "r");

	if(!input_file) {
		if(filename) {
			char *str, *cpy;
			str = strerror(errno);
			if(str) {
				cpy = malloc(strlen(str) + 1);
				if(cpy) {
					strcpy(cpy, str);
					error->dyn_msg = cpy;
				}
			}
			error->static_msg = "couldn't load file";
		}else {
			error->static_msg = "trying to load NULL filename\n";
		}
		return -1;
	}

	assert(interp);
	assert(input_file);

	YY_BUFFER_STATE new_buf = yy_create_buffer (input_file, YY_BUF_SIZE);
	yypush_buffer_state(new_buf);

/*
 * FIXME:
 *
 * This interface has to be changed, I want reentrant/thread safe, but
 * anyhow, this is ok for now.  Just load all interpreters in the same
 * thread.
 */
	top = NULL;
	current_interp = interp;
	FIAL_parser_error_info = error;

	yylloc.line = 1;
	yylloc.col  = 1;

	FIAL_parser_line_no = 1;
	FIAL_parser_col_no  = 1;


	tmp = yyparse();
	yypop_buffer_state();
	fclose(input_file);
	input_file = NULL;

	if(tmp != 0 || !top) {
		error->file = filename;
		return -1;
	}

	if( add_lib_from_ast(interp, top, filename, &lib_ent, error) == -1) {
		error->file = filename;
		return -1;
	}

	/*FIXME: running load stuff. */

	if(lib_ent)
		*ret_lib = lib_ent;

	return 0;
}

/* this will perfrom a library lookup, returns 0 on success, 1 if it
   couldn't find it.  Doesn;t actuall load anything, called "load"
   because it uses load parameter conventions, interp-label-lib_entry.
   Does not modify ret_lib on failure.  */

int FIAL_load_lookup (struct FIAL_interpreter *interp,
		      const char *lib_label,
		      union FIAL_lib_entry **ret_lib)
{
	union FIAL_lib_entry *iter;
	for(iter = interp->libs; iter != NULL; iter = iter->stub.next) {
		if(strcmp(iter->stub.label, lib_label) == 0) {
			*ret_lib = iter;
			return 0;
		}
	}
	return 1;
}


/* this will lookup the string label first, and if it is available, it
   will simply return the library. */

int FIAL_load_string (struct FIAL_interpreter    *interp,
		      const char              *lib_label,
	              union FIAL_lib_entry     **new_lib,
		      struct FIAL_error_info      *error)
{
	if(!FIAL_load_lookup(interp, lib_label, new_lib))
		return 1;

	return  FIAL_load_file(interp, lib_label, new_lib, error);
}

/*FIXME:
 *
 *  Refactor FIAL_set_symbol and this routine to eliminate code
 *  duplication.
 */

/* this doesn't finalize, and overwrites references */

int FIAL_set_symbol_raw (symbol_map *m, int sym,
			 value const *val)
{
	struct FIAL_symbol_map_entry *tmp = NULL, *iter = NULL;

	for(iter = m->first;
	    iter != NULL;
	    iter = iter->next) {
		if (sym == iter->sym) {
			iter->val = *val;
			return 0;
		}
	}

	assert(iter == NULL);

	tmp = ALLOC(sizeof(*tmp));
	if(!tmp) {
		return -1;
	}

	memset(tmp, 0, sizeof(*tmp));
	tmp->sym = sym;
	tmp->val = *val;
	tmp->next = m->first;

	m->first = tmp;
	return 1;
}

/*this craetes a copy of everything it uses, which gets deleted with
 * the interpreter.  Makes everything easy.  This will currently
 * silently add a duplicate, so make sure it isn't there to begin
 * with!  Don't rely on the current behavior of this shoving the new
 * lib on top of the stack, leaving the old one never to be seen
 * again... */

/* lib_def is a null terminated array of FIAL_c_function_defs, I'm
 * doing it this way because these are easy to statically define..*/

/* ok, real rewrite starts here.  I guess my plan goes out the window
   and I just rewrite this mother. */

/* returns -1 on allocation error */

int FIAL_load_c_lib (struct FIAL_interpreter    *interp,
		     char                       *lib_label,
		     struct FIAL_c_func_def     *lib_def,
		     struct FIAL_c_lib         **new_lib)
{
	struct FIAL_c_func           *tmp      = NULL;
	struct FIAL_c_func_def       *def_iter = NULL;
	union  FIAL_lib_entry        *lib_ent  = NULL;
	struct FIAL_c_lib            *lib      = NULL;
	struct FIAL_value             val;
	FIAL_symbol                   sym      = 0;

	/*FIXME: ALLOC_ERRORS*/
	/*FIXME: error reporting, like, any.  But dooing errors
	 * properly is next on my list, so half assing it here doens't
	 * make any difference.  */

	lib_ent = ALLOC(sizeof(*lib_ent));
	memset(lib_ent, 0, sizeof(*lib_ent));

	lib = &lib_ent->c_lib;
	lib->type = FIAL_LIB_C;
	assert((void *)(lib == (void *)lib_ent) && lib != NULL);

	assert(lib_label);
	lib->label = STRING_ALLOC(strlen(lib_label) + 1);
	if(!lib->label)
		return -1;
	strcpy(lib->label, lib_label);
	lib->funcs = FIAL_create_symbol_map();

	for(def_iter = lib_def; def_iter->func != NULL; def_iter++) {
		memset(&val, 0, sizeof(val));
		sym = 0;

		tmp = ALLOC(sizeof(*tmp));
		if(!tmp)
			return -1;
		memset(tmp, 0, sizeof(*tmp));

		tmp->func = def_iter->func;
		tmp->ptr  = def_iter->ptr;

		FIAL_get_symbol(&sym, def_iter->name, interp);
		val.type = VALUE_C_FUNC;
		val.func = tmp;

		if(FIAL_set_symbol_raw(lib->funcs, sym, &val) == -1)
			return -1;
	}

	if(new_lib)
		*new_lib = lib;
	lib->next = interp->libs;
	interp->libs = lib_ent;

	return 0;
}

/*
 * FIXME:
 * This shouldn't be here, i should make a file with outward facing C
 * stuff, and use this just for loading, but whatevs.
 */

#define INITIAL_TYPE_TABLE_SIZE 50

#if VALUE_USER > INITIAL_TYPE_TABLE_SIZE
#error VALUE_USER cannot be larger than INITIAL_TYPE_TABLE_SIZE
#endif /*VALUE_MAP >= INITIAL_TYPE_TABLE_SIZE*/

static int map_dp_wrapper (struct FIAL_value *a,
		       struct FIAL_interpreter *b,
		       void   *c)
{
	return FIAL_destroy_symbol_map_value(a, b);
}

static int array_dp_wrapper (struct FIAL_value *a,
			     struct FIAL_interpreter *b,
			     void   *c)
{
	return FIAL_destroy_array_value(a, b);
}

struct FIAL_finalizer map_fin = {map_dp_wrapper, NULL};
struct FIAL_finalizer array_fin = {array_dp_wrapper, NULL};

/* return s-1 on allocation error */

static inline int init_types (struct FIAL_master_type_table *mtt)
{
	size_t size;

	mtt->finalizers = ALLOC((size = sizeof(*(mtt->finalizers)) *
				  INITIAL_TYPE_TABLE_SIZE));
	if(!mtt->finalizers) {
		return -1;
	}

	mtt->cap = INITIAL_TYPE_TABLE_SIZE;
	mtt->size = VALUE_USER;

	assert(size);
	memset(mtt->finalizers, 0, size);

	mtt->finalizers[VALUE_MAP]   = map_fin;
	mtt->finalizers[VALUE_ARRAY] = array_fin;

/*
 * Add extra type finishers here.
 */
	return 0;
}

int FIAL_add_omni_lib (struct FIAL_interpreter *interp,
		       FIAL_symbol              sym,
		       struct FIAL_c_lib      *lib)
{
	struct FIAL_symbol_map_entry *entry = NULL;

	if(!interp->omnis) {
		interp->omnis = FIAL_create_symbol_map();
	}

	assert(interp->omnis);
	entry = ALLOC(sizeof(*entry));
	memset(entry, 0, sizeof(*entry));

	entry->sym            = sym;
	entry->val.type       = VALUE_C_LIB;
	entry->val.c_lib      = lib;
	entry->next           = interp->omnis->first;
	interp->omnis->first  = entry;

	return 0;
}

struct FIAL_interpreter *FIAL_create_interpreter ()
{
	interpreter *interp = ALLOC(sizeof(*interp));
	memset(interp, 0, sizeof(*interp));
	init_types(&interp->types);
	return interp;
}
