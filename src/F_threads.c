/* 
 * I am going to have to do some optomizing here, this is a lot of one line
 * functions, etc, that I don't know if it makes sense putting them in here.  
 */

#include "F_threads.h"

#ifdef WIN32 

int FIAL_init_lock (FIAL_LOCK *crit_sec)
{
	
	if (!InitializeCriticalSectionAndSpinCount(crit_sec, 
	                                           0x00000400) ) 
		return -1;
	return 0;
}

void FIAL_deinit_lock (FIAL_LOCK *crit_sec)
{
	if(!crit_sec) 
		return;
	DeleteCriticalSection(crit_sec);
}

void FIAL_lock_lock (FIAL_LOCK *c)
{
	EnterCriticalSection(c); 
}

int FIAL_lock_try(FIAL_LOCK *lock)
{
	return TryEnterCriticalSection(lock);
}

void FIAL_lock_unlock (FIAL_LOCK *lock)
{
	LeaveCriticalSection (lock);
}

int FIAL_init_sem (FIAL_SEM *sem, int count)
{
	sem->count = count;
	if (!InitializeCriticalSectionAndSpinCount(&sem->lock, 
	                                           0x00000400) ) 
		return -1;
	InitializeConditionVariable(&sem->cond);

	return 0;
}

void FIAL_deinit_sem(FIAL_SEM *sem)
{
	if(!sem)
		return;
	DeleteCriticalSection(&sem->lock);
	memset(sem, 0, sizeof(*sem));
}

void FIAL_sem_post (FIAL_SEM *sem)
{
	EnterCriticalSection(&sem->lock);
	sem->count++;
	LeaveCriticalSection(&sem->lock);
	WakeConditionVariable(&sem->cond);
}

void FIAL_sem_pend (FIAL_SEM *sem)
{
	EnterCriticalSection(&sem->lock);
	while (sem->count <= 0) 
		SleepConditionVariableCS(&sem->cond, &sem->lock, INFINITE);
	sem->count--;
	LeaveCriticalSection(&sem->lock);
}

/* the try applies to the semaphore aspect, this will wait on the lock
   no matter what. */

int FIAL_sem_try(FIAL_SEM *sem)
{
	EnterCriticalSection(&sem->lock);
	if(sem->count > 0) {
		sem->count--;
		LeaveCriticalSection(&sem->lock);
		return 1;
	}
	LeaveCriticalSection(&sem->lock);
	return 0;
}


int FIAL_sem_get_count(FIAL_SEM *sem)
{
	int tmp;
	EnterCriticalSection(&sem->lock);
	tmp = sem->count;
	LeaveCriticalSection(&sem->lock);
	return tmp;
}

#else /* WIN32 */

#error "attempting to build WIN32 sync primatives without WIN32 defined"

#endif /* WIN32 */
