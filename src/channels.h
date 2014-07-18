#ifndef FIAL_CHANNELS_H
#define FIAL_CHANNELS_H
/*
 * I am just hard coding this to deal with FIAL_values, I will have
 * to add stuff later for other stuff.  I might have to rethink this a tad too,
 * I don't think there is an easy way to add counting here, for finishers.  
 *
 * But, actually, "forcing" closed channels is not going to be easy anyway, so I
 * can assume that it is uncontested, in which case I can just check the value
 * of the "take" semaphore.....
 *
 * TODO:
 *  (1) use a pointer passing interface for put/take.  My "demo" code was using 
 *      ints, that's the only reason it is set up this way.  Might want to
 *      profile before and after, since I'm not sure the speed up will be all
 *      that significant.  
 */

#include "FIAL.h"
#include "F_threads.h"

struct FIAL_pick_package;
struct FIAL_channel {
	int refs;
	int id;
	int size;
	struct FIAL_interpreter *interp;
	struct FIAL_pick_package *mailbox;
	FIAL_LOCK lock, put_lock, take_lock, ref_lock;
	FIAL_SEM put, take;
	struct FIAL_value *first, *last;
	struct FIAL_value value[1];
};

int FIAL_init_channel (struct FIAL_channel *chan, struct FIAL_interpreter *);
void FIAL_deinit_channel (struct FIAL_channel *chan);
struct FIAL_channel *FIAL_create_channel(int size, struct FIAL_interpreter *);
void FIAL_destroy_channel (struct FIAL_channel *chan);
void FIAL_channel_dec_refs (struct FIAL_channel *chan);
void FIAL_channel_inc_refs (struct FIAL_channel *chan);
void FIAL_channel_put (struct FIAL_channel *chan, 
                        struct FIAL_value value);
struct FIAL_value FIAL_channel_take (struct FIAL_channel *chan);
int FIAL_channel_pick (struct FIAL_value *value, int *picked_id,
                  struct FIAL_channel **channel_choices, 
                  int ch_array_size);

#endif /* FIAL_CHANNEL_H */
