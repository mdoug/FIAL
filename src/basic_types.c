#include <stdlib.h>
#include <string.h>

#include "basic_types.h"
#include "interp.h"

#include "value_def_short.h"

/*no way around it, I need to finalize replaced values in the set
  symbol routine. */

typedef struct FIAL_symbol_map symbol_map;
typedef struct FIAL_value value;

/* I guess I am still using these, but I expect to just get rid of
   them and use normal malloc calls -- if I need to, I can always
   implement a new malloc or download one or something.  */

#define MAP_ALLOC(x)    malloc(x)
#define MAP_FREE(x)     free(x)
#define ENTRY_ALLOC(x)  malloc(x)
#define ENTRY_FREE(x)   free(x)
#define ALLOC_ERROR(x)  assert((x, 0))
#define ALLOC(x)        malloc(x)
#define REALLOC(x, y)   realloc(x, y)
#define FREE(x)         free(x)
#define ERROR(x)        assert((x, 0))

symbol_map * FIAL_create_symbol_map (void)
{
	symbol_map *m = MAP_ALLOC(sizeof(*m));
	if(!m) {
		return NULL;
	}
	memset (m, 0, sizeof(*m));
	return m;
}

int FIAL_destroy_symbol_map_value (struct FIAL_value *value,
				   struct FIAL_interpreter *interp)
{
	assert(value->type = VALUE_MAP);
	return FIAL_destroy_symbol_map (value->ptr, interp);
}

int FIAL_destroy_symbol_map (struct FIAL_symbol_map *kill_me,
			     struct FIAL_interpreter *interp)
{
	struct FIAL_symbol_map_entry *iter, *tmp;
	struct FIAL_finalizer        *fin = NULL;

	if(!kill_me)
		return 0;
	assert(interp->types.finalizers);
	for(iter = kill_me->first; iter != NULL; iter = tmp) {
		assert(iter);
		tmp = iter->next;
		if((fin = interp->types.finalizers + iter->val.type)->func)
		    fin->func(&iter->val, interp, fin->ptr);
	}
	ENTRY_FREE(iter);
	MAP_FREE(kill_me);
	return 0;
}

typedef struct FIAL_exec_env exec_env;

static inline int set_symbol_finalize (value *val, exec_env *env)
{
	struct FIAL_finalizer *fin;
	assert(val->type < env->interp->types.size);
	fin = env->interp->types.finalizers +  val->type;
	if(fin->func)
		fin->func(val, env->interp, fin->ptr);
	return 0;
}

int FIAL_set_symbol(symbol_map *m, int sym, value const *val,
		    exec_env *env)
{
	struct FIAL_symbol_map_entry *tmp = NULL, *iter = NULL;

	for(iter = m->first;
	    iter != NULL;
	    iter = iter->next) {
		if (sym == iter->sym) {
			value *tmp;
			if(iter->val.type == VALUE_REF)
				tmp = iter->val.ref;
			else
				tmp = &iter->val;

			set_symbol_finalize(tmp, env);
			*tmp = *val;
			return 0;
		}
	}

	assert(iter == NULL);

	tmp = ENTRY_ALLOC(sizeof(*tmp));
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

int FIAL_lookup_symbol(value *val, symbol_map *m, int sym)
{
	struct FIAL_symbol_map_entry *iter = NULL;
	assert(m);
	if(m->first) {
		for(iter = m->first;
		    iter != NULL;
		    iter = iter->next) {
			if (sym == iter->sym) {
				if(iter->val.type == VALUE_REF)
					*val = *iter->val.ref;
				else
					*val = iter->val;
				return 0;
			}
		}
	}
	assert(iter == NULL);

	memset(val, 0, sizeof(*val));
	return 1;
}


/* actually, this makes no sense.  The purpose of this must be to set
 * the value to none, not to clear it...*/

int FIAL_map_lookup_and_clear(value *val, symbol_map *m, int sym,
	                      struct FIAL_interpreter *interp )
{
	struct FIAL_symbol_map_entry  *iter = NULL;
	assert(m);
	if(m->first) {
		for(iter = m->first;
		    iter != NULL;
		    iter = iter->next) {
			if (sym == iter->sym) {
				if(iter->val.type == VALUE_REF)
					*val = *iter->val.ref;
				else
					*val = iter->val;
				memset(&iter->val, 0, sizeof(iter->val));
				return 0;
			}
		}
	}
	assert(iter == NULL);

	memset(val, 0, sizeof(*val));
	return 1;
}


/*************************************************************
 *
 * ARRAYS.
 *
 *  note -- no factory function for arrays, just memset to 0;
 *
 *************************************************************/

int FIAL_destroy_array(struct FIAL_array *array,
		       struct FIAL_interpreter *interp)
{
	struct FIAL_value *iter, *end;
	struct FIAL_finalizer *fin;

	assert(interp);
	if(!array) {
		return 1;
	}
	if(array->size == 0) {
		FREE(array->array);
		FREE(array);
		return 0;
	};
	assert(array->array);
	for(iter = array->array, end = array->array + array->size;
	    iter < end; ++iter) {
		assert(iter->type < interp->types.size);
		fin = interp->types.finalizers + iter->type;
		if(fin->func)
			fin->func(iter, interp, fin->ptr);

	}
	FREE(array->array);
	FREE(array);

	return 0;
}

int FIAL_destroy_array_value(struct FIAL_value *val,
			     struct FIAL_interpreter *interp)
{

	assert(val->type == VALUE_ARRAY);
	FIAL_destroy_array(val->array, interp);
	memset(val, 0, sizeof(*val));
	return 0;
}

/* returns -1 on allocation error.

  returns -2 if passed a negative integer ( i could just make it take
   unsigned ints, but since int is the value type, that's what i'm
   going with.  )
*/

int FIAL_array_set_index (struct FIAL_array *array, size_t index,
			  struct FIAL_value *val,
			  struct FIAL_interpreter *interp)
{
	assert(array);
	if(index < 1) {
		ERROR("only positive indexes allowed!");
		return -1;
	}
	if(index > array->size) {
		if(index > array->cap) {
			struct FIAL_value *tmp;
			size_t new_cap = ((index > array->cap * 2) ?
					  array->cap * 2 : index);
			new_cap = new_cap > 20 ? new_cap : 20;
			tmp = REALLOC(array->array,
				      new_cap * sizeof(struct FIAL_value));
			if(!tmp) {
				ERROR("couldnt' get space to add value!");
				return -1;
			}
			array->array = tmp;
			memset(array->array + array->cap, 0,
	  		    sizeof(struct FIAL_value) * (new_cap - array->cap));
			array->cap = new_cap;
		}
	} else {
		FIAL_clear_value(array->array + index - 1, interp);
	}
	array->array[index - 1] = *val;
	memset(val, 0, sizeof(*val));

	if(array->size < index)
		array->size = index;

	return 0;
}

int FIAL_array_get_index (struct FIAL_value *val, struct FIAL_array *array,
			  size_t index, struct FIAL_interpreter *interp)
{
	assert(array);
	if(index < 1) {
		ERROR("only positive indexes allowed!");
		return -1;
	}
	if(index > array->size){
		FIAL_clear_value(val, interp);
		val->type = VALUE_ERROR;
		return 1;
	}
	assert(array->size);
	assert(array->array);
	*val = array->array[index - 1];

	/*FIXME NEED COPYING / moving */
	memset((array->array + (index - 1)), 0, sizeof(struct FIAL_value));

	return 0;
}

/* returns:

-1 on bad alloc.

*/

int FIAL_register_type(int *new_type,
		       struct FIAL_finalizer *fin,
		       struct FIAL_interpreter *interp)
{
	assert(interp->types.size <= interp->types.cap);

	if(interp->types.size == interp->types.cap) {
		struct FIAL_finalizer *tmp = NULL;
		size_t new_size = interp->types.cap * 2 * sizeof(*tmp);

		tmp = realloc(interp->types.finalizers, new_size);
		if(!tmp) {
			return -1;
		}
		memset(&interp->types.finalizers[interp->types.cap],
		       0, new_size / 2);
		interp->types.cap *= 2;
		interp->types.finalizers = tmp;
	}

	*new_type    = interp->types.size;
	if(fin)
		interp->types.finalizers[interp->types.size] = *fin;
	interp->types.size++;

	return 0;
}
