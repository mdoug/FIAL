#include <string.h>
#include <assert.h>

#include "interp.h"
#include "api.h"
#include "basic_types.h"
#include "loader.h"

#include "error_def_short.h"
#include "value_def_short.h"

#define DEFAULT_SEQ_SIZE 24

struct FIAL_seq *FIAL_create_seq (void)
{
	struct FIAL_seq *seq;
	seq = calloc(sizeof(*seq), 1);
	return seq;
}

/* should this deallocate the memory?  certianly not if it is done in
   destory, just not sure a "clear" function should be deallocating.
   I don't think it is obvious that it does.... */

void FIAL_clear_seq (struct FIAL_seq *seq,
		     struct FIAL_interpreter *interp)
{

	struct FIAL_value *iter;
	assert(seq->first <= seq->head);
	for(iter = seq->first; iter != seq->head; iter++)
		FIAL_clear_value(iter, interp);

	seq->first = seq->head = seq->seq;
}

void FIAL_destroy_seq (struct FIAL_seq *seq,
		       struct FIAL_interpreter *interp)
{
	if(!seq)
		return;

	FIAL_clear_seq(seq, interp);

	free(seq->seq);
	free(seq);
}

static int seq_grow (struct FIAL_seq *seq)
{
	if(!seq->seq) {
		seq->size = DEFAULT_SEQ_SIZE;
		seq->seq  = calloc(sizeof(*seq->seq), DEFAULT_SEQ_SIZE);
		if(!seq->seq)
			return -1;
		seq->first = seq->head = seq->seq;
	} else {
		int offset = seq->first - seq->seq;
		int count  = seq->head  - seq->first;
		struct FIAL_value *tmp;

		/* always grow by 2 */
		tmp = realloc(seq->seq, sizeof(*seq->seq) * seq->size * 2);
		if(tmp == NULL)
			return -1;
		seq->seq   = tmp;
		seq->first = seq->seq + offset;
		seq->head  = seq->first + count;
		seq->size  = seq->size * 2;
	}
	return 0;
}

static void seq_move_over (struct FIAL_seq *seq)
{
	int count = seq->head - seq->first;
	memmove(seq->seq, seq->first, count * sizeof(*seq));
	memset(seq->seq + count, 0,
	       sizeof(*seq->seq) * (seq->size - count));
	seq->first = seq->seq;
	seq->head  = seq->seq + count;

}

static int need_room (struct FIAL_seq *seq)
{

	/*
	 * this number is a bit arbitrary, not sure it's right -- just
	 * want a simple heuristic to know when to move over and when
	 * to reallocate
	 */

	if(seq->first - seq->seq > seq->size / 3) {
		seq_move_over(seq);
	} else {
		if(seq_grow(seq)) {
			if(seq->first != seq->seq) {
				seq_move_over(seq);
			} else {
				return -1;
			}
		}
	}
	return 0;
}

int FIAL_seq_in (struct FIAL_seq *seq,
		 struct FIAL_value *val)
{
	assert(seq->head != val);
	assert(seq->head - seq->seq <= seq->size);

	if(seq->head - seq->seq == seq->size)
		if (need_room(seq) != 0)
			return -1;

	assert(seq->head - seq->seq < seq->size);

	/* I was using FIAL_move_value here, but it requires an
	   interpter as argument, since it clears the destination.
	   But I know the destination is free, so I don't need to do
	   that. */

	memcpy((seq->head)++, val, sizeof(*seq->head));
	memset(val, 0, sizeof(*val));

	return 0;
}

int FIAL_seq_first(struct FIAL_value *val,
		   struct FIAL_seq   *seq,
		   struct FIAL_interpreter *interp)
{
	assert(seq->first <= seq->head);
	if(seq->first == seq->head)
		return -1;
	FIAL_move_value(val, (seq->first)++, interp);
	return 0;
}

int FIAL_seq_last(struct FIAL_value *val,
		  struct FIAL_seq   *seq,
		  struct FIAL_interpreter *interp)
{
	assert(seq->first <= seq->head);
	if(seq->first == seq->head)
		return -1;
	FIAL_move_value(val, --(seq->head), interp);
	return 0;
}

int FIAL_seq_copy (struct FIAL_seq *to,
		   struct FIAL_seq *from,
		   struct FIAL_interpreter *interp)
{
	struct FIAL_value *tmp;
	struct FIAL_value *iter;
	struct FIAL_value none;

	memset(&none, 0, sizeof(none));
	assert(to && from && interp);

	FIAL_clear_seq(to, interp);

	if(to->size < (from->head - from->first)) {
		tmp = realloc(to->seq, sizeof(*to->seq)
			      * (from->head - from->seq));
		if(!tmp)
			return -1;
		to->first = to->head = to->seq = tmp;
		to->size = from->head - from->seq;
		memset(to->seq, 0, sizeof(*to->seq) * to->size);
	}

	for(iter = from->first; iter != from->head; ++iter,(to->head)++ ) {
		if(FIAL_copy_value(to->head, iter, interp) < 0)
			return -1;
	}
	return 0;
}

/*
 * FIAL interface functions
 */

static int seq_create (int argc, struct FIAL_value **argv,
		       struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_seq *seq = NULL;

	if(argc < 1) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "Argument to put new sequence in "
			"required.";
		FIAL_set_error(env);
		return -1;
	}
	assert(argv);
	FIAL_clear_value(argv[0], env->interp);
	seq = FIAL_create_seq();
	if(!seq) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = "unable to allocate sequence.";
		FIAL_set_error(env);
		return -1;
	}
	argv[0]->type = VALUE_SEQ;
	argv[0]->seq  = seq;

	return 0;
}

static int seq_in (int argc, struct FIAL_value **argv,
		   struct FIAL_exec_env *env, void *ptr)
{
	int i;
	enum {SEQ = 0};

	if(argc < 2 || argv[0]->type != VALUE_SEQ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"To put item in sequence, arg1 must be a sequence, it "
			"must be\nfollowed by one or more items";
		FIAL_set_error(env);
		return -1;
	}
	assert(argv);
	for(i = 0; i < (argc - 1); i++) {
		if(FIAL_seq_in(argv[SEQ]->seq, argv[i+1]) < 0) {
			env->error.code = ERROR_BAD_ALLOC;
			env->error.static_msg = "Unable to add space to "
				                "sequence for new value.";
			FIAL_set_error(env);
			return -1;
		}
	}
	return 0;
}

static int seq_first (int argc, struct FIAL_value **argv,
		      struct FIAL_exec_env *env, void *ptr)
{
	enum {SEQ = 1};

	if(argc < 2 || argv[SEQ]->type != VALUE_SEQ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"To get first element, need argument to place value "
			"and sequence from which to get value";
		FIAL_set_error(env);
		return -1;
	}
	if(FIAL_seq_first(argv[0], argv[1]->seq, env->interp) < 0) {
		env->error.code = ERROR_CONTAINER_ACCESS;
		env->error.static_msg =
			"Attempt to get item from empty sequence";
		FIAL_set_error(env);
		return -1;
	}
	return 0;
}


static int seq_last (int argc, struct FIAL_value **argv,
		      struct FIAL_exec_env *env, void *ptr)
{
	enum {SEQ = 1};

	if(argc < 2 || argv[SEQ]->type != VALUE_SEQ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"To get first element, need argument to place value "
			"and sequence from which to get value";
		FIAL_set_error(env);
		return -1;
	}
	if(FIAL_seq_last(argv[0], argv[1]->seq, env->interp) < 0) {
		env->error.code = ERROR_CONTAINER_ACCESS;
		env->error.static_msg =
			"Attempt to get item from empty sequence";
		FIAL_set_error(env);
		return -1;
	}
	return 0;
}

static int seq_take(int argc, struct FIAL_value **argv,
		    struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_value ** const a = argv;
	enum {SEQ = 1, INDEX = 2};
	if(argc < 3 || argv[SEQ]->type != VALUE_SEQ ||
	   argv[INDEX]->type != VALUE_INT ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Taking a sequence element requires a return argument, "
			"a sequence, and an integral index, respectively.";
		FIAL_set_error(env);
		return -1;
	}
	if(INDEX[a]->n < 1 ||
	   INDEX[a]->n > (SEQ[a]->seq->head - SEQ[a]->seq->first)) {
		env->error.code = ERROR_CONTAINER_ACCESS;
		env->error.static_msg =
			"Attempt to access out of range index in sequence";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_move_value(argv[0], SEQ[a]->seq->first + (INDEX[a]->n - 1),
			env->interp);
	return 0;
}

static int seq_put(int argc, struct FIAL_value **argv,
		   struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_value ** const a = argv;
	enum {SEQ = 0, INDEX = 1};
	if(argc < 3 || argv[SEQ]->type != VALUE_SEQ ||
	   argv[INDEX]->type != VALUE_INT ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Putting to a sequence requires a sequence, an index, "
			"and a value to put";
		FIAL_set_error(env);
		return -1;
	}
	if(INDEX[a]->n < 1 ||
	   INDEX[a]->n > (SEQ[a]->seq->head - SEQ[a]->seq->first)) {
		env->error.code = ERROR_CONTAINER_ACCESS;
		env->error.static_msg =
			"Attempt to access out of range index in sequence";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_move_value(SEQ[a]->seq->first + (INDEX[a]->n - 1),
			argv[2],
			env->interp);
	return 0;
}


static int seq_dupe(int argc, struct FIAL_value **argv,
		    struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_value ** const a = argv;
	enum {SEQ = 1, INDEX = 2};
	if(argc < 3 || argv[SEQ]->type != VALUE_SEQ ||
	   argv[INDEX]->type != VALUE_INT ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
			"Taking a sequence element requires a return argument, "
			"a sequence, and an integral index, respectively.";
		FIAL_set_error(env);
		return -1;
	}
	if(INDEX[a]->n < 1 ||
	   INDEX[a]->n > (SEQ[a]->seq->head - SEQ[a]->seq->first)) {
		env->error.code = ERROR_CONTAINER_ACCESS;
		env->error.static_msg =
			"Attempt to access out of range index in sequence";
		FIAL_set_error(env);
		return -1;
	}
	FIAL_copy_value(argv[0], SEQ[a]->seq->first + (INDEX[a]->n - 1),
			env->interp);
	return 0;
}

static int seq_size (int argc, struct FIAL_value **args,
			 struct FIAL_exec_env *env,
			 void *ptr)
{
	int size;
	if(argc < 2) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "no arguments to seq size -- need "
		                        "return value and array.";
		FIAL_set_error(env);
		return -1;
	}

	if(args[1]->type != VALUE_SEQ) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = "attempt to use sequence size for something "
					"other than sequence";
		FIAL_set_error(env);
		return -1;
	}

	assert(args);

	size = args[1]->seq->head - args[1]->seq->first;
	FIAL_clear_value(args[0], env->interp);

	args[0]->type = VALUE_INT;
	args[0]->n    = size;

	return 0;

}


static int seq_finalize (struct FIAL_value *val,
			 struct FIAL_interpreter *interp,
			 void *ptr)
{
	assert(val);
	assert(val->type = VALUE_SEQ);
	FIAL_destroy_seq(val->seq, interp);
	return 0;
}

static int seq_copy (struct FIAL_value *to,
		     struct FIAL_value *from,
		     struct FIAL_interpreter *interp,
		     void *ptr)
{
	if (to == from)
		return 0;
	FIAL_clear_value(to, interp);

	assert(from->type == VALUE_SEQ);
	to->type = VALUE_SEQ;
	to->seq  = FIAL_create_seq();
	if(!to->seq)
		return -1;
	return FIAL_seq_copy (to->seq, from->seq, interp);
}

int FIAL_install_seq (struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def lib_seq[] =
		{
			{"create"  , seq_create , NULL},
			{"in"      , seq_in     , NULL},
			{"first"   , seq_first  , NULL},
			{"last"    , seq_last   , NULL},
			{"take"    , seq_take   , NULL},
			{"put"     , seq_put    , NULL},
			{"size"    , seq_size   , NULL},
			{"dupe"    , seq_dupe   , NULL},
			{NULL, NULL, NULL}
		};

	struct FIAL_finalizer fin = {seq_finalize, NULL};
	struct FIAL_copier    cpy = {seq_copy,     NULL};

	if( FIAL_load_c_lib (interp, "sequence", lib_seq, NULL) < 0)
		return -1;
	interp->types.finalizers[VALUE_SEQ] = fin;
	interp->types.copiers[VALUE_SEQ] = cpy;
	return 0;
}
