#ifndef FIAL_BASIC_TYPES_H
#define FIAL_BASIC_TYPES_H

#include <stdlib.h>

/*
 * defines are generated automatically from value_defines.json, using
 * the gen_defines.py script in ../utils.  Different versions are
 * available, depending on name requirements (i.e. with and without
 * the FIAL_ prefix.)
 */

/*************************************************************
 *
 * Declarations
 *
 *************************************************************/

typedef int FIAL_symbol;
struct FIAL_value;
struct FIAL_symbol_map;
struct FIAL_array;

/*************************************************************
 *
 * Prototypes
 *
 *************************************************************/

struct FIAL_exec_env;
struct FIAL_interpreter;
struct FIAL_finalizer;
struct FIAL_copier;
struct FIAL_symbol_map * FIAL_create_symbol_map (void);

int FIAL_destroy_symbol_map (struct FIAL_symbol_map *kill_me,
			     struct FIAL_interpreter *interp);
int FIAL_copy_symbol_map (struct FIAL_symbol_map *to,
			  struct FIAL_symbol_map *from,
			  struct FIAL_interpreter *interp);
int FIAL_destroy_symbol_map_value (struct FIAL_value *value,
				   struct FIAL_interpreter *interp);
/*returns 0 if new, 1 if old, -1 on error */
int FIAL_set_symbol(struct FIAL_symbol_map *m,
		    int sym,
		    struct FIAL_value const *val,
		    struct FIAL_interpreter *);
/*returns 0 if available, 1 if not, -1 on error */
int FIAL_lookup_symbol(struct FIAL_value *val,
		       struct FIAL_symbol_map *m,
		       int sym);
int FIAL_map_lookup_and_clear (struct FIAL_value *val,
			       struct FIAL_symbol_map *m,
			       int sym, struct FIAL_interpreter *interp);

int FIAL_destroy_array(struct FIAL_array *, struct FIAL_interpreter *);
int FIAL_destroy_array_value(struct FIAL_value *, struct FIAL_interpreter *);

int FIAL_register_type(int *new_type,
		       struct FIAL_finalizer *fin,
		       struct FIAL_copier *cpy,
		       struct FIAL_interpreter *interp);



/************************************************************
 *
 * Definitions
 *
 *************************************************************/

struct FIAL_ast_node;
struct FIAL_text_buf;

/* oh shit.  this is not standard C  Oh shit.  Oh shit.  */

/* ok, deep breaths, I can change this.  but, there is no way to
   change it, without changing a shitload of stuff.  So I will leave
   it as is.  I think msvc supports these, and clang too.  They are
   part of the C11 standard.  I am ok.  I would not have done this
   though, everything else is ansi C.  Sigh.

   But, in retrospect, this has made everything much, much easier.
   So, there is that.

 */

#ifndef FIAL_VALUE_USER_FIELDS
#define FIAL_VALUE_USER_FIELDS
#endif

struct FIAL_proc;
struct FIAL_error_info;

struct FIAL_value {
	int type;
	union {
		int n;
		float x;
		FIAL_symbol sym;
		struct FIAL_symbol_map  *map;
		struct FIAL_value       *ref;
		struct FIAL_ast_node   *node;
		struct FIAL_c_func     *func;
		struct FIAL_library     *lib;
		struct FIAL_c_lib     *c_lib;
		struct FIAL_text_buf   *text;
		struct FIAL_seq         *seq;
		struct FIAL_proc       *proc;
		struct FIAL_error_info  *err;
		struct FIAL_channel    *chan;
		char *str;
		void *ptr;
		FIAL_VALUE_USER_FIELDS
	};
};

struct FIAL_symbol_map_entry {
	int sym;
	struct FIAL_value val;
	struct FIAL_symbol_map_entry *next;
};

struct FIAL_symbol_map {
	struct FIAL_symbol_map_entry *first;
};

struct FIAL_seq {
	int size;
	struct FIAL_value *seq;
	struct FIAL_value *first;
	struct FIAL_value *head;
};

struct FIAL_text_buf {
	char *buf;
	int len;
	int cap;
};

#endif /*FIAL_BASIC_TYPES_H*/
