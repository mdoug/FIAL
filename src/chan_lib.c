#include <assert.h>

#include "FIAL.h"
#include "channels.h"


#define SET_ERROR_MSG(env, e_code, msg) do {          \
		(env)->error.code = (e_code);       \
		(env)->error.static_msg=(msg);    \
		FIAL_set_error(env);                \
	} while(0)

static int create (int argc, struct FIAL_value **argv, 
                   struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_channel *chan;

	(void) ptr;
	if (argc < 1 ||
	    (argc > 1 &&
	     argv[1]->type != VALUE_INT)) {
		env->error.code = ERROR_INVALID_ARGS;
		env->error.static_msg = 
		"Need (1) return value, and optionally, (2), a size, to"
		"create a channel";
		FIAL_set_error(env);
		return -1;
	}
	assert(argc == 1 || argv[1]->type == VALUE_INT);
	chan = FIAL_create_channel(argc > 1 ? argv[1]->n : 0, env->interp);
	if(!chan) {
		env->error.code = ERROR_BAD_ALLOC;
		env->error.static_msg = 
		"Couldn't create channel";
		FIAL_set_error(env);
		return -1;
	}	
	FIAL_clear_value(argv[0], env->interp);
	argv[0]->type = VALUE_CHANNEL;
	argv[0]->chan = chan;
	return 0;
}

static int put (int argc, struct FIAL_value **argv, 
                struct FIAL_exec_env *env, void *ptr)
{
	int i;
	struct FIAL_channel *chan;

	(void) ptr;
	if (argc < 2 ||
	    argv[0]->type != VALUE_CHANNEL) {
		SET_ERROR_MSG(env, ERROR_INVALID_ARGS,
		"To put to channel, need (1) channel, followed by one or more \
		values to put");
		return -1;
	}
	chan = argv[0]->chan;
	for (i = 1; i < argc; i++) {
		FIAL_channel_put(chan, *argv[i]);
		memset(argv[i], 0, sizeof(*argv[i]));
	}
	return 0;
}

static int take(int argc, struct FIAL_value **argv, 
                struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_value tmp;

	(void) ptr;
	if(argc < 2 ||
	   argv[1]->type != VALUE_CHANNEL) {
		SET_ERROR_MSG(env, ERROR_INVALID_ARGS,	
		"To take from channel, need (1) return value, (2) channel");
		return -1;	
	}
	tmp = FIAL_channel_take(argv[1]->chan);
	FIAL_clear_value(argv[0], env->interp);
	*argv[0] = tmp;
	return 0;
}

/*
 * note to self: maybe pick would be the better name for the non-blocking,
 * simpler, method.  Though alternatives to the simpler one are "try", "check",
 * "snag", "now", and I guess alternatives for the more complex one are
 * "select", "wait"
 */

static int pick(int argc, struct FIAL_value **argv,
                struct FIAL_exec_env *env, void *ptr)
{
	struct FIAL_channel **pick_ems;
	struct FIAL_channel **pkms_iter;
	struct FIAL_seq *seq;
	struct FIAL_value *iter;

	struct FIAL_value value;
	int id;
	int size;
	int ret;

	(void) ptr;
	if (argc < 3 ||
	    argv[2]->type != VALUE_SEQ ||
	    !argv[2]->seq) {
		SET_ERROR_MSG(env, ERROR_INVALID_ARGS,
		"Pick on a channel requires return values for (1) value, (2)"
		"picked channel index.\n Also requres (3) seq of channels.");
		return -1;
	}
	seq = argv[2]->seq;
	size = seq->head - seq->first;
	pick_ems = malloc(sizeof(*pick_ems) * size);
	for(pkms_iter = pick_ems, iter = seq->first; iter != seq->head; 
	    pkms_iter++, iter++) {
		if(iter->type != VALUE_CHANNEL) {
			free (pick_ems);
			SET_ERROR_MSG(env, ERROR_INVALID_ARGS,
			"All items in sequence to pick must be channels");
			return -1;
		}
		*pkms_iter = iter->chan;
	}

	/* not sure what the best here is, just returning none values.  I can
         * change later, none as id is not a bad error indicator though i don'
         * think.*/

	if (FIAL_channel_pick(&value, &id, pick_ems, size) < 0) {
		free(pick_ems);
		FIAL_clear_value(argv[0], env->interp);
		FIAL_clear_value(argv[1], env->interp);
		return 1;
	}	
	free(pick_ems);
	FIAL_clear_value(argv[0], env->interp);
	FIAL_clear_value(argv[1], env->interp);
	*argv[0] = value;
	argv[1]->type = VALUE_INT;
	argv[1]->n = (id + 1);
	return 0;
}

/* FIXME:
 *
 * this could use a refactor, not sure why I decided to put the interpreter into
 * the channel structure, when I could handle it here.
 *
 * My head hasn't been screwed on straight for a couple days I don't think, so
 * it goes. Trying to do the whole "channel.c/channel.h" library where types
 * could be redefined, etc, was just a waste I think.  
 */

static int final (struct FIAL_value *val, struct FIAL_interpreter *interp, 
                  void *ptr) 
{
	(void) ptr;

	assert (val->type == VALUE_CHANNEL);
	assert (val->chan->interp == interp);
	
	FIAL_channel_dec_refs(val->chan);
	memset(val, 0, sizeof(*val));
}

static int copy (struct FIAL_value *to, struct FIAL_value *from, 
                 struct FIAL_interpreter *interp, void *ptr)
{
	(void) ptr;
	if (to == from)
		return 0;
	FIAL_clear_value(to, interp);
	FIAL_channel_inc_refs(from->chan);
	*to = *from;
	return 0;
}

int FIAL_install_channels(struct FIAL_interpreter *interp)
{
	struct FIAL_c_func_def def[] = {
		{"create", create, NULL},
		{"put"   , put,    NULL},
		{"take"  , take,   NULL},
		{"pick"  , pick,   NULL},
		{NULL    , NULL,   NULL}
	};
	struct FIAL_finalizer f = {final, NULL};
	struct FIAL_copier    c = {copy,  NULL};

	if(FIAL_load_c_lib(interp, "channels", def, NULL) < 0)
		return -1;

	assert(interp->types.size > VALUE_CHANNEL);
	interp->types.finalizers[VALUE_CHANNEL] = f;
	interp->types.copiers[VALUE_CHANNEL] = c;

	return 0;
}
