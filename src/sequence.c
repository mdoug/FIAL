#include <string.h>
#include <assert.h>

#include "interp.h"
#include "api.h"
#include "basic_types.h"
#include "loader.h"

#include "error_def_short.h"
#include "value_def_short.h"

#define DEFAULT_SEQ_SIZE 12

/*
 * I should add a routine that sets allocates a certain amount of
 * memory at some point.  Didn't percieve the need at first, but it
 * would be useful for initializers and for a possible make tuple
 * proc.
 */

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

int FIAL_seq_reserve (struct FIAL_seq *seq, int new_size)
{
	int current_size;
	int actual_size;
	int offset;
	struct FIAL_value *tmp;

	assert(seq);
	offset = seq->first - seq->seq;
	current_size = seq->head - seq->first;
	actual_size = seq->size - offset;
	if ((seq->size - offset) >= new_size) 
		return 0;
	
	if (actual_size >= new_size) {
		memmove(seq->seq, seq->first, current_size * sizeof(*seq->seq));
		seq->first -= offset;
		seq->head -= offset;
		memset(seq->head, 0, sizeof(*seq->head) * 
		       (seq->size - current_size));
		assert(seq->first == seq->seq && 
		       seq->head - seq->first == actual_size);
		return 0;
	}

	memmove(seq->seq, seq->first, actual_size * sizeof(*seq->seq));
	tmp = realloc(seq->seq, sizeof(*seq->seq) * new_size);
	if(tmp == NULL) {
		seq->head = (seq->head) - (seq->first - seq->seq);
		seq->first = seq->seq;
		return -1;
	}
	seq->seq = tmp;
	seq->size = new_size;
	seq->first = seq->seq;
	seq->head = seq->seq + current_size;
	memset(seq->head, 0, sizeof(*seq->head) * (seq->size - current_size));
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

int FIAL_seq_append (struct FIAL_seq *to, struct FIAL_seq *from)
{
	int tmp;

	assert(to && from);
	if(from->head - from->first == 0)
		return 0;

	assert(from->head && from->first && from->seq);

	tmp = FIAL_seq_reserve(to, (to->head - to->first) + 
	                           (from->head - from->first));
	if(tmp < 0) 
		return -1;

	/* guaranteed not to overlap, even if to and from are the same, since
         * head is outside of the array */

	memcpy (to->head, from->first, 
	         sizeof(*to->head) * (from->head - from->first));
	to->head += from->head - from->first;
	
	if (to != from) {
		memset(from->first, 0, 
		       (sizeof *from->first) * (from->head - from->first));
		from->first = from->head = from->seq;
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

	(void)ptr;
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

static void append_arg_error (struct FIAL_exec_env *env)
{
	env->error.code = ERROR_INVALID_ARGS;
	env->error.static_msg =
		"To append sequences, arg1 must be a seq, it must be "
		"followed by seqs to append";
	FIAL_set_error(env);
}

static int seq_append (int argc, struct FIAL_value **argv,
		   struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_seq *to;
	int i;
	enum {SEQ = 0};

	(void)ptr;
	if(argc < 2 || 
	   argv[0]->type != VALUE_SEQ ||
	   argv[1]->type != VALUE_SEQ) {
		append_arg_error (env);
		return -1;
	}
	assert(argv);
	to = argv[0]->seq;
	for(i = 1; i < argc; i++) {
		if (argv[i]->type != VALUE_SEQ) {
			if(FIAL_seq_in(argv[SEQ]->seq, argv[i+1]) < 0) {
				env->error.code = ERROR_BAD_ALLOC;
				env->error.static_msg = 
				"Unable to add space to seq for new value.";
				FIAL_set_error(env);
				return -1;
			}
		} else {
			struct FIAL_seq *from = argv[i]->seq;
			assert(argv[i]->type == VALUE_SEQ);
			if (FIAL_seq_append (to, from) < 0) {
				env->error.code = ERROR_BAD_ALLOC;
				env->error.static_msg = 
				"Unable to append, out of memory";
				FIAL_set_error(env);
				return -1;
			}
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


/* this will add (n) new none values onto the end of the sequence */

static int seq_expand (int argc, struct FIAL_value **argv,
                       struct FIAL_exec_env *env, void *ptr)
{
	if (argc < 2 ||
	    argv[0]->type != VALUE_SEQ ||
	    argv[1]->type != VALUE_INT) { 
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg =
		"Need (1) sequence, and (2) int size of sequence expansion to"
		"expand sequence";
		FIAL_set_error(env);
		return -1;
	}
	if (FIAL_seq_reserve(argv[0]->seq, argv[1]->n) < 0) { 
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = 
		"Unable to expand, out of memory";
		FIAL_set_error(env);
		return -1;
	}
	argv[0]->seq->head += argv[1]->n;
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
	struct FIAL_c_func_def lib_seq[] = {
			{"create"  , seq_create , NULL},
			{"in"      , seq_in     , NULL},
			{"append"  , seq_append , NULL},
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
