#ifndef __TEST_VERIFY_THREAD_WRAPPERS_PTHREAD_H
#define __TEST_VERIFY_THREAD_WRAPPERS_PTHREAD_H

#include <pthread.h>
#include <assert.h>

typedef int bool;
#define false 0
#define true 1

typedef bool (*COND_FUNC)(int *);
static bool arg_is_one(int *arg) { return *arg==1; }
static bool arg_is_zero(int *arg) { return *arg==0; }
static bool arg_is_true(int *arg) { return *arg==true; }

typedef struct Lock {
	pthread_mutex_t *m_;
	pthread_cond_t *cv_;
	bool signal_at_unlock_;
}Lock;

void lock(Lock *lk) 
{
	assert(0==pthread_mutex_lock(lk->m_));
}

void unlock(Lock *lk)
{
	if(lk->signal_at_unlock_)
		assert(0==pthread_cond_signal(lk->cv_));
	assert(0==pthread_mutex_unlock(lk->m_));
}

void lock_when_unlock(Lock *lk,COND_FUNC func,int *cond)
{
	lock(lk);
	lk->signal_at_unlock_=true;
	while(!func(cond))
		pthread_cond_wait(lk->cv_,lk->m_);
	unlock(lk);
}

typedef struct RWLock {
	pthread_rwlock_t *rw_;
}RWLock;

void rwlock_wrlock(RWLock *rwlk) 
{
	assert(0==pthread_rwlock_wrlock(rwlk->rw_));
}

bool rwlock_trywrlock(RWLock *rwlk)
{
	return pthread_rwlock_trywrlock(rwlk->rw_)==0;
}

void rwlock_rdlock(RWLock *rwlk)
{
	assert(0==pthread_rwlock_rdlock(rwlk->rw_));
}

bool rwlock_tryrdlock(RWLock *rwlk)
{
	return pthread_rwlock_tryrdlock(rwlk->rw_)==0;
}

void rwlock_unlock(RWLock *rwlk)
{
	pthread_rwlock_unlock(rwlk->rw_);
}

typedef struct Barrier {
	pthread_barrier_t b_;
}Barrier;
void barrier_init(Barrier *b,int val)
{
	pthread_barrier_init(&b->b_,0,val);
}
void barrier_wait(Barrier *b)
{
	pthread_barrier_wait(&b->b_);
}
void barrier_destroy(Barrier *b) {
	pthread_barrier_destroy(&b->b_);
}

typedef struct CondVar {
	pthread_cond_t *cv_;
}CondVar;
void cond_wait(CondVar *cv,Lock *lk) 
{
	pthread_cond_wait(cv->cv_,lk->m_);
}

void cond_signal(CondVar *cv)
{
	pthread_cond_signal(cv->cv_);
}

void cond_singal_all(CondVar *cv)
{
	pthread_cond_broadcast(cv->cv_);
}

typedef void *(*worker_t)(void *);
typedef struct Thread {
	pthread_t tid_;
	void *arg_;
	void *entry_;
}Thread;
void thread_init(Thread *thd,void *entry,void *arg) {
	thd->entry_=entry;
	thd->arg_=arg;
}
void thread_start(Thread *thd)
{
	pthread_attr_t attr;
	assert(0 == pthread_attr_init(&attr));
	assert(0 == pthread_attr_setstacksize(&attr, 2*1024*1024));
    assert(0 == pthread_attr_setdetachstate(&attr, false));
    assert(0 == pthread_create(&thd->tid_, &attr, (worker_t)thd->entry_,thd->arg_));
    assert(0 == pthread_attr_destroy(&attr));
}
void thread_join(Thread *thd)
{
	pthread_join(thd->tid_,NULL);
}
#endif //__TEST_VERIFY_THREAD_WRAPPERS_PTHREAD_H