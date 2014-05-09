#include <stdlib.h>
#include <string.h>

#include "basic_types.h"
#include "interp.h"
#include "api.h"  /* this is just for clear_value, maybe some
		   * refactoring is in order. */

#include "value_def_short.h"

/* FIXME: clean this up, errors, etc.  OR, delete it, in favor of sequences */

/*no way around it, I need to finalize replaced values in the set
  symbol routine. */

typedef struct FIAL_symbol_map symbol_map;
typedef struct FIAL_value value;

/*
 * These were made with the best of intentions, but this is just crazy
 * at this point.  I've stopped using them, I should eliminate them.
 *
 * Obviously, I wouldn't be adverse to having ways to set up custom
 * allocators, but it will be easier to get the data structures the
 * way I want them first, and then figure out whether to allow
 * overriding the allocators.
 *
 * At this point, this isn't just going against the "you aren't going
 * to need it" principle, which I am not entirely sure I agree with,
 * it is also going against the KISS principle, which I am adopting
 * whole heartedly.
 */

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

void FIAL_clear_symbol_map (struct FIAL_symbol_map *empty_me,
			    struct FIAL_interpreter *interp)
{

	struct FIAL_symbol_map_entry *iter, *tmp;
	struct FIAL_finalizer        *fin = NULL;

	for(iter = empty_me->first; iter != NULL; iter = tmp) {
		assert(iter);
		tmp = iter->next;
		if((fin = interp->types.finalizers + iter->val.type)->func)
			fin->func(&iter->val, interp, fin->ptr);
		free(iter);
	}
	empty_me->first = NULL;
}

int FIAL_destroy_symbol_map (struct FIAL_symbol_map *kill_me,
			     struct FIAL_interpreter *interp)
{

	if(!kill_me)
		return 0;
	assert(interp->types.finalizers);
	FIAL_clear_symbol_map(kill_me, interp);
	MAP_FREE(kill_me);
	return 0;
}

/* these return -1 on out of mem */

/* FIXME --- */

/* I think i botched this up, I end up having to reverse the list.
 * Not too worried, this list thing isn't going to be around forever.
 * Might come back and fix it later, I have a lot to do right now
 * and fatigue is setting in.  */

int FIAL_copy_symbol_map (struct FIAL_symbol_map *to,
			  struct FIAL_symbol_map *from,
			  struct FIAL_interpreter *interp)
{
	struct FIAL_symbol_map_entry *iter, *to_iter, *tmp;

	assert(to && from);

	if(to == from)
		return 0;
	FIAL_clear_symbol_map(to, interp);

	for (iter = from->first, to_iter = NULL;
	     iter != NULL;
	     iter = iter->next) {
		tmp = calloc (sizeof(*tmp), 1);
		if(!tmp) {
			/* just attach what we have and quit */
			to->first = to_iter;
			return -1;
		}
		tmp->next = to_iter;
		to_iter = tmp;
		to_iter->sym = iter->sym;
		FIAL_copy_value (&to_iter->val, &iter->val, interp);
	}

	/* reverse them, so they're in the same order as from order */
	for(iter = NULL; to_iter != NULL; to_iter =  tmp) {
		tmp  = to_iter->next;
		to_iter->next = iter;
		iter = to_iter;
	}
	to->first = iter;
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
	assert(m);

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
 * the value to none, not to clear it.  have to rename these
 * functions, FIAL_lookup_symbol should be FIAL_map_something_or_other
 * as well.  */

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

#ifdef KEEP_ARRAYS

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

#endif /* KEEP_ARRAYS */

/* returns:

-1 on bad alloc.
-2 if interp is not set to "load";

*/

int FIAL_register_type(int *new_type,
		       struct FIAL_finalizer *fin,
		       struct FIAL_copier    *cpy,
		       struct FIAL_interpreter *interp)
{
	assert(interp->types.size <= interp->types.cap);

	if(interp->state != FIAL_INTERP_STATE_LOAD)
		return -2;

	if(interp->types.size == interp->types.cap) {
		struct FIAL_finalizer *tmp = NULL;
		struct FIAL_copier    *tmp2 = NULL;
		size_t new_size = interp->types.cap * 2 * sizeof(*tmp);

		tmp = realloc(interp->types.finalizers, new_size);
		if(!tmp) {
			return -1;
		}
		interp->types.finalizers = tmp;
		memset(&interp->types.finalizers[interp->types.cap],
		       0, new_size / 2);

		tmp2 = realloc(interp->types.copiers, new_size);
		if(!tmp2) {
			return -1;
		}
		interp->types.copiers = tmp2;
		memset(&interp->types.copiers[interp->types.cap],
		       0, new_size / 2);

		interp->types.cap *= 2;
	}

	*new_type    = interp->types.size;
	if(fin)
		interp->types.finalizers[interp->types.size] = *fin;
	if(cpy)
		interp->types.copiers[interp->types.size] = *cpy;
	interp->types.size++;

	return 0;
}
