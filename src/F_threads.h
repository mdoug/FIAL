/* this file was called F_threads.h.  But, since it deals with synchronization,  * it will be renamed to sync.h  */


/* this file declares FIAL_LOCK and FIAL_SEM, they are all caps because they are
 * typedefs, I guess?  They were originally defines, but that doesn't make any
 * sense, I must have been brain dead when I did that. So I changed to typedef,
 * but kept the all caps.  */

#ifndef FIAL_F_THREADS_H
#define FIAL_F_THREADS_H

#ifdef WIN32
#include <Windows.h>

typedef CRITICAL_SECTION FIAL_LOCK;

/*
 * yes, I know windows has semaphores, but (a) they are "heavy weight", i.e.
 * inter process, and (b) you need need to increment the semaphore in order to
 * get the value, which I need to get in order to know how many values to
 * finalize, and that seemed a bit hacky to me.  So, I am just implementing my
 * own via condition variables. 
 */

struct FIAL_semaphore {
	int count;
	CRITICAL_SECTION lock;
	CONDITION_VARIABLE cond;
};

typedef struct FIAL_semaphore FIAL_SEM;

#else
#include <pthread.h>

#if !defined(PTHREADS)
#error "Need WIN32 or PTHREADS defined to build"
#endif

typedef pthread_mutex_t FIAL_LOCK;

/* actually, I think posix semaphores are also interprocess, so I will just do
 * the same thing here as I did for windows.  */

struct FIAL_semaphore {
	int count;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};

typedef struct FIAL_semaphore FIAL_SEM;

#endif /* WIN32 */


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
