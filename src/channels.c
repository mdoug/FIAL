/*
 * Well, I was going to make a sort of preprocessor driven way to deal with
 * different types of values, but it got too mucky, so I will just go ahead and
 * deal with it using FIAL values.  For my purposes its fine to just hard code
 * it all in here anyway.  
 */

/* 
 * (3) cnosider switching from Windows mutexes, to critical sections.  Mutexes
 *     are for cross process synchronization, which of course works inside the
 *     process, but slower.
 */

#ifdef WIN32_THREADS
#define WIN32_LEAN_AND_MEAN /* I think it was talk like a pirate day when 
                               they named this.  */

#include <Windows.h>
#endif 

#include <stdio.h>  /* console app */
#include <stdlib.h>
#include <string.h>

#include "channels.h"

struct FIAL_pick_package {
	int id;
	struct FIAL_value value;
	FIAL_SEM bi;
	FIAL_SEM done;
};

int static make_a_channel (struct FIAL_channel *chan, 
                           struct FIAL_interpreter *interp,
                           int size)
{
	int tmp = 0;
	chan->first = chan->last = chan->value;
	chan->refs = 1;
	chan->size = size;
	chan->interp = interp;

	if(FIAL_init_lock(&chan->lock)) {
		tmp = 1;
		goto error;
	}
	if (FIAL_init_lock(&chan->put_lock)) {
		tmp = 2;
		goto error;
	}
	if (FIAL_init_lock(&chan->take_lock)) {
		tmp = 3;
		goto error;
	}
	if (FIAL_init_lock(&chan->ref_lock)) {
		tmp = 4;
		goto error;
	}
	if (FIAL_init_sem(&chan->put, 0)) {
		tmp = 5;
		goto error;
	}
	if (FIAL_init_sem(&chan->take, size)) {
		tmp = 6;
		goto error;
	}
	return 0;
error:
	/* deinit in reverse order, fallthrough gets everything before the error
         */

	switch(tmp) {
	case 6:
		FIAL_deinit_sem(&chan->take);
	case 5:
		FIAL_deinit_sem(&chan->put);
	case 4:
		FIAL_deinit_lock(&chan->ref_lock);
	case 3:
		FIAL_deinit_lock(&chan->take_lock);
	case 2:
		FIAL_deinit_lock(&chan->put_lock);
	case 1:
		FIAL_deinit_lock(&chan->lock);
		break;
	default:
		assert(0);
	}
	memset(chan, 0, sizeof(*chan));
	return -1;
}

int FIAL_init_channel (struct FIAL_channel *chan, struct FIAL_interpreter *interp)
{
	memset(chan, 0, sizeof(*chan));
	return make_a_channel(chan, interp, 0);
}

/* 
 * I am assuming a deinit happens in an uncontested way.  I.e, last reference
 * has been dereferenced.  If this is *not* the case, well, sophisticated
 * mechanizations would be in order.  At that point, currently, you have to just
 * bail out entirely.  
 */

void FIAL_deinit_channel (struct FIAL_channel *chan)
{
	int count;
	int len;
	int i = 0;
	count = FIAL_sem_get_count(&chan->take);
	len = chan->size - count;

	for(i = 0; i < len; i++) {
		FIAL_clear_value(chan->first, chan->interp);
		if (chan->first != chan->value + chan->size)
			chan->first++;
		else 
			chan->first = chan->value;
	}
	FIAL_deinit_lock(&chan->lock);
	FIAL_deinit_lock(&chan->put_lock);
	FIAL_deinit_lock(&chan->take_lock);
	FIAL_deinit_lock(&chan->ref_lock);
	FIAL_deinit_sem(&chan->put);
	FIAL_deinit_sem(&chan->take);
	memset(chan, 0, sizeof(*chan));
}

void FIAL_destroy_channel (struct FIAL_channel *chan)
{
	FIAL_deinit_channel (chan);
	free(chan);
}

/* Note:  don't use reference counting on channels held on the stack.  That just 
 *        leads to disaster, since it calls free (it _has_ to, there is nowhere
 *        else that could call it after the last reference is gone....)  Instead, make 
 *        sure there aren't any references, and call deinit directly. 
 */

void FIAL_channel_dec_refs (struct FIAL_channel *chan)
{
	FIAL_lock_lock (&chan->ref_lock);
	if(--chan->refs == 0) {
		FIAL_lock_unlock(&chan->ref_lock);
		FIAL_destroy_channel(chan);
		return;
	}
	FIAL_lock_unlock(&chan->ref_lock);
}

void FIAL_channel_inc_refs (struct FIAL_channel *chan)
{
	FIAL_lock_lock (&chan->ref_lock);
	chan->refs++;
	FIAL_lock_unlock (&chan->ref_lock);
}

struct FIAL_channel *FIAL_create_channel (int size, struct FIAL_interpreter *interp)
{
	size_t data_size;  
	struct FIAL_channel *chan; 

	data_size = sizeof(*chan) + sizeof(*chan->value) * size;
	chan = calloc(data_size, 1);
	if (!chan)
		return NULL;
	memset(chan, 0, data_size);
	if (make_a_channel (chan, interp, size) != 0) {
		free(chan);
		return NULL;
	}
	return chan;
}

/* these functions are internal, they don't do any checking either.  
   Use semaphores or condition variables for counting */

static void put_value (struct FIAL_channel *chan, struct FIAL_value value)
{
	*chan->last = value;
	if(chan->last == chan->value + chan->size)
		chan->last = chan->value;
	else
		chan->last++;
}

static struct FIAL_value take_value (struct FIAL_channel *chan)
{
	struct FIAL_value tmp = *chan->first;
	if(chan->first == chan->value + chan->size)
		chan->first = chan->value;
	else 
		chan->first++;
	return tmp;
}

/* This function does all the heavy lifting -- this enables picks to 
 * be done without any thread overhead.  Otherwise, it would have to be
 * done separately, and would require one thread per pickable object.  
 * That strikes me as too "heavy" of a thing, for what is a primitive 
 * operation (within this scheme, that is).
 */

void FIAL_channel_put (struct FIAL_channel *chan, 
                       struct FIAL_value value)
{
	FIAL_lock_lock(&chan->put_lock);
	FIAL_lock_lock(&chan->lock);
	if(!chan->mailbox) {
		put_value (chan, value);
		FIAL_sem_post(&chan->put);
		FIAL_lock_unlock(&chan->lock);
		FIAL_sem_pend(&chan->take);
		FIAL_lock_unlock(&chan->put_lock);
		return;
	}
	if(!FIAL_sem_try(&chan->mailbox->bi)) {
		put_value(chan, value);
		FIAL_sem_post(&chan->put);
		FIAL_lock_unlock(&chan->lock);
		FIAL_sem_pend(&chan->take);
		FIAL_lock_unlock(&chan->put_lock);
		return;
	}
	chan->mailbox->id = chan->id;
	chan->mailbox->value = value;
	FIAL_sem_post(&chan->mailbox->done);
	FIAL_lock_unlock(&chan->lock);
	FIAL_sem_pend(&chan->take);
	FIAL_lock_unlock(&chan->put_lock);
	return;
}

struct FIAL_value FIAL_channel_take (struct FIAL_channel *chan)
{
	struct FIAL_value tmp;

	FIAL_lock_lock(&chan->take_lock);
	FIAL_sem_pend(&chan->put);

	tmp = take_value(chan);

	FIAL_sem_post(&chan->take);
	FIAL_lock_unlock(&chan->take_lock);
	return tmp;
}

/* ok, this is going to destroy the array that is passed in.  Otherwise,
   I would have to spend a malloc in here, to get a copy of it, since
   I don't have variable length arrays, since I'm on msvc.  
 
   Thogh maybe alloca would be an acceptable substitute.  
   
   Oh hell, this thing is slow anyway, I'm just goint to eat a malloc for 
   now.  When I need more optomization I will probably go for alloca.  
*/

/* FIXME: this mallocs a copy of channel_choices, no matter what,
 *        which could represent a serious performance hit in some
 *        circumstances 
 */

int FIAL_channel_pick (struct FIAL_value *value, int *picked_id,
                       struct FIAL_channel **channel_choices, 
                       int ch_array_size)
{
	int i;
	int count = 0;
	struct FIAL_pick_package mbox;
	struct FIAL_channel **ch_array = malloc(sizeof(*ch_array) * 
	                                  ch_array_size);
	if(!ch_array) {
		return -1;
	}
	memcpy(ch_array, channel_choices, sizeof(*ch_array) * 
	                                  ch_array_size);
	memset(&mbox, 0, sizeof(mbox));
	FIAL_init_sem(&mbox.bi, 1);
	FIAL_init_sem(&mbox.done, 0);

	for (i = 0; i < ch_array_size; i++) {
		if (FIAL_lock_try(&ch_array[i]->take_lock) == 0) {
			ch_array[i] = NULL;
			continue;
		}
		FIAL_lock_lock(&ch_array[i]->lock);
		if (FIAL_sem_try(&ch_array[i]->put)) {
			int j;
			for(j = 0; j < i; j++) {
				if (!ch_array[j])
					continue;
				ch_array[j]->mailbox = NULL;
				FIAL_lock_unlock(&ch_array[j]->lock);
				FIAL_lock_unlock(&ch_array[j]->take_lock);
			}
			*value = take_value(ch_array[i]);
			*picked_id = i;
			FIAL_sem_post(&ch_array[i]->take);
			FIAL_lock_unlock(&ch_array[i]->lock);
			FIAL_lock_unlock(&ch_array[i]->take_lock);
		
			free(ch_array);
			FIAL_deinit_sem(&mbox.bi);
			FIAL_deinit_sem(&mbox.done);
			return 0;
		}
		count++;
		ch_array[i]->id = i;
		ch_array[i]->mailbox = &mbox;

	}
	if(!count) {
		free(ch_array);
		FIAL_deinit_sem(&mbox.bi);
		FIAL_deinit_sem(&mbox.done);
		return -1;
	}
	/* ok, no values found, just going to unlock these 
	  suckers, and then wait on my semaphore.  */

	for(i = 0; i < ch_array_size; i++)
		FIAL_lock_unlock(&ch_array[i]->lock);
	FIAL_sem_pend(&mbox.done);
	*value = mbox.value;
	*picked_id = mbox.id;
	for(i = 0; i < ch_array_size; i++) {
		if(!ch_array[i])
			continue;
		FIAL_lock_lock(&ch_array[i]->lock);
		ch_array[i]->mailbox = NULL;
		FIAL_lock_unlock(&ch_array[i]->lock);
		FIAL_lock_unlock(&ch_array[i]->take_lock);
	}
	FIAL_sem_post(&ch_array[mbox.id]->take);
	FIAL_deinit_sem(&mbox.bi);
	FIAL_deinit_sem(&mbox.done);
	free (ch_array);
	return 0;
}

