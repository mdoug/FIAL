/* * Actually, all the "thread" functions are just in the proc.c file for now,
 * this deals with synchronization primatives.  
 */

#ifndef FIAL_F_THREADS_H
#define FIAL_F_THREADS_H

#define WIN32_THREADS
#ifdef WIN32_THREADS

#include <Windows.h>
#define FIAL_LOCK CRITICAL_SECTION 

/*
 * yes, I know windows has semaphores, but (a) they are "heavy weight", i.e.
 * inter process, and (b) you need need to increment the semaphore in order to
 * get the value, which I need to get in order to know how many values to
 * finalize, and that seemed a bit hacky to me.  So, I am just implementing my own via 
 * condition variables. 
 */

struct FIAL_semaphore {
	int count;
	CRITICAL_SECTION lock;
	CONDITION_VARIABLE cond;
};

#define FIAL_SEM struct FIAL_semaphore

#endif /* WIN32_THREADS */

/* 
 * TODO: benchmark these, I think I am hurting performance by having these
 * little functions in their own file.  Not sure *how* to go about getting a
 * reasonable benchmark on that, though.
 */

int FIAL_init_lock (FIAL_LOCK *);
void FIAL_deinit_lock (FIAL_LOCK *);
void FIAL_lock_lock (FIAL_LOCK *);
int FIAL_lock_try(FIAL_LOCK *lock);
void FIAL_lock_unlock (FIAL_LOCK *lock);
int FIAL_init_sem (FIAL_SEM *, int count);
void FIAL_deinit_sem(FIAL_SEM *);
void FIAL_sem_post (FIAL_SEM *sem);
void FIAL_sem_pend (FIAL_SEM *sem);
int FIAL_sem_try(FIAL_SEM *sem);
int FIAL_sem_get_count(FIAL_SEM *sem);

#endif /* FIAL_F_THREADS_H */
