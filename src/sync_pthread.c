#include "F_threads.h"

#ifndef PTHREADS
#error "Attempt to use pthread sync primiatives without PTHREADS defined"
#endif

int FIAL_init_lock (FIAL_LOCK *lock)
{
	if (pthread_mutex_init (lock, NULL) != 0)
		return -1;
	return 0;
}

void FIAL_deinit_lock (FIAL_LOCK *lock)
{
	pthread_mutex_destroy (lock);
}

void FIAL_lock_lock (FIAL_LOCK *lock)
{
	pthread_mutex_lock (lock);
}

void FIAL_lock_unlock (FIAL_LOCK *lock)
{
	pthread_mutex_unlock (lock);
}
/* returns zero on failure, nonzero on success */
int FIAL_lock_try (FIAL_LOCK *lock)
{
	return !(pthread_mutex_trylock (lock));
}

int FIAL_init_sem (FIAL_SEM *sem, int count)
{
	sem->count = count;
	if (pthread_mutex_init (&sem->lock, NULL) != 0)
		return -1;
	if (pthread_cond_init (&sem->cond, NULL) != 0) { 
		pthread_mutex_destroy (&sem->lock);
		return -1;
	}
	return 0;
}

void FIAL_deinit_sem (FIAL_SEM *sem)
{
	if (!sem)
		return;
	pthread_cond_destroy (&sem->cond);
	pthread_mutex_destroy (&sem->lock);
}

void FIAL_sem_post (FIAL_SEM *sem)
{
	pthread_mutex_lock (&sem->lock);
	sem->count++;
	pthread_mutex_unlock (&sem->lock);
	pthread_cond_signal(&sem->cond);
}

void FIAL_sem_pend (FIAL_SEM *sem)
{
	pthread_mutex_lock (&sem->lock);
	while (sem->count <= 0)
		pthread_cond_wait (&sem->cond, &sem->lock);
	sem->count--;
	pthread_mutex_unlock (&sem->lock);
}

/* the try applies to the semaphore aspect, this will wait on the lock
   no matter what. */

int FIAL_sem_try (FIAL_SEM *sem)
{
	pthread_mutex_lock (&sem->lock);
	if (sem->count > 0) {
		sem->count--;
		pthread_mutex_unlock(&sem->lock);
		return 1;
	}
	pthread_mutex_unlock (&sem->lock);
	return 0;
}

int FIAL_sem_get_count (FIAL_SEM *sem)
{
	int tmp;
	pthread_mutex_lock (&sem->lock);
	tmp = sem->count;
	pthread_mutex_unlock (&sem->lock);
	return tmp;
}


