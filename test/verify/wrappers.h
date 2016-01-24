#ifndef __TEST_VERIFY_WRAPPERS_H
#define __TEST_VERIFY_WRAPPERS_H

//all the wrapper functions are used for cetus static analysis

#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "lf_queue.h"

int pthread_attr_init(pthread_attr_t *) { return 0; }
int pthread_attr_destroy(pthread_attr_t *) { return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *, int) { return 0; }
int pthread_attr_setstacksize(pthread_attr_t *, size_t) { return 0; }

int pthread_create(pthread_t *tidp,const pthread_attr_t *attr,
	void *(*start_rtn)(void*),void *arg)
{ return 0; }
int pthread_join(pthread_t thread, void **retval) { return 0; }

int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *)
{ return 0; }
int pthread_mutex_lock(pthread_mutex_t *mutex) { return 0; }
int pthread_mutex_unlock(pthread_mutex_t *mutex) { return 0; }
int pthread_mutex_trylock( pthread_mutex_t *mutex ) { return 0; }
int pthread_mutex_destroy(pthread_mutex_t *) { return 0; }

int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *)
{ return 0; }
int pthread_cond_signal(pthread_cond_t *cond) { return 0; }
int pthread_cond_broadcast(pthread_cond_t *cond) { return 0; }
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{ return 0; }
int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
	const struct timespec *) { return 0; }
int pthread_condattr_destroy(pthread_condattr_t *) { return 0; }

int pthread_rwlock_init(pthread_rwlock_t *rwlock,
    const pthread_rwlockattr_t *attr) { return 0; }
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) { return 0; }
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock) { return 0; }
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) { return 0; }
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock) { return 0; }
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) { return 0; }
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) { return 0; }

int pthread_barrier_init(pthread_barrier_t * barrier,
	const pthread_barrierattr_t *attr, unsigned count) 
{ return 0; }
int pthread_barrier_destroy(pthread_barrier_t *barrier) { return 0; }
int pthread_barrier_wait(pthread_barrier_t *barrier) { return 0; }

int sem_init(sem_t *sem, int pshared, unsigned int value) { return 0; }
int sem_destroy(sem_t *sem) { return 0; }
int sem_post(sem_t *sem) { return 0; }
int sem_wait(sem_t *sem) { return 0; }
int sem_trywait(sem_t *sem) { return 0; }
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{ return 0; }

void *pthread_getspecific(pthread_key_t key) { return NULL; }
int pthread_setspecific(pthread_key_t key, const void *value) { return 0; }

int sched_yield() { return 0; }


int mkstemp(char *template) { return 0; }
char *strdup(const char *s1) { return NULL; }
int close(int fd) { return 0; }
int usleep(useconds_t usec) { return 0; }
int closedir(DIR *dirp) { return 0; }
DIR *opendir(const char *name) { return NULL; }
void *mmap(void *addr, size_t length, int prot, int flags,
	int fd, off_t offset) { return NULL; }
int munmap(void *addr, size_t length) { return 0; }

#endif

