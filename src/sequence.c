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

void FIAL_destroy_seq (struct FIAL_seq *seq,
		       struct FIAL_interpreter *interp)
{
	struct FIAL_value *iter;

	if(!seq)
		return;

	assert(seq->first <= seq->head);
	for(iter = seq->first; iter != seq->head; iter++)
		FIAL_clear_value(iter, interp);

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
	}else {
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

static int seq_finalize (struct FIAL_value *val,
			 struct FIAL_interpreter *interp,
			 void *ptr)
{
	assert(val);
	assert(val->type = VALUE_SEQ);
	FIAL_destroy_seq(val->seq, interp);
	return 0;
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
			{NULL, NULL, NULL}
		};

	struct FIAL_finalizer fin = {seq_finalize, NULL};

	if( FIAL_load_c_lib (interp, "sequence", lib_seq, NULL) < 0)
		return -1;
	interp->types.finalizers[VALUE_SEQ] = fin;
	return 0;
}
