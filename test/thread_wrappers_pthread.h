#ifndef __TEST_THREAD_WRAPPERS_PTHREAD_H
#define __TEST_THREAD_WRAPPERS_PTHREAD_H

#include <unistd.h>
#include <pthread.h>
#include <assert.h>

static bool ArgIsOne(int *arg) { return *arg == 1; };
static bool ArgIsZero(int *arg) { return *arg == 0; };
static bool ArgIsTrue(bool *arg) { return *arg == true; };


// Just a boolean condition. Used by Mutex::LockWhen and similar.
class Condition {
public:
	typedef bool (*func_t)(void *);

	template <typename T>
	Condition(bool (*func)(T *),T *arg)
	:func_(reinterpret_cast<func_t>(func)),arg_(arg) {}

	Condition(bool (*func)())
	:func_(reinterpret_cast<func_t>(func)),arg_(NULL) {}

	bool Eval() { return func_(arg_);}
private:
	func_t func_;
	void *arg_;
};


// Wrapper for pthread_mutex_t.

// pthread_mutex_t is *not* a reader-writer lock,
// so the methods like ReaderLock() aren't really reader locks.
// We can not use pthread_rwlock_t because it
// does not work with pthread_cond_t.

// TODO: We still need to test reader locks with this class.
// Implement a mode where pthread_rwlock_t will be used
// instead of pthread_mutex_t (only when not used with CondVar or LockWhen).

class Mutex {
public:
	Mutex() {
		assert(0==pthread_mutex_init(&mu_,NULL));
		assert(0==pthread_cond_init(&cv_,NULL));
		signal_at_unlock_=false;
	}
	~Mutex() {
		assert(0==pthread_cond_destroy(&cv_));
		assert(0==pthread_mutex_destroy(&mu_));
	}
	void Lock() { assert(0==pthread_mutex_lock(&mu_)); }
	bool TryLock() { return (0==pthread_mutex_trylock(&mu_)); }
	void Unlock() {
		if(signal_at_unlock_) {
			assert(0==pthread_cond_signal(&cv_));			
		}
		assert(0==pthread_mutex_unlock(&mu_));
	}
	//not really reader locks
	void ReaderLock() { Lock(); }
	bool ReaderTryLock() { return TryLock(); }
	void ReaderUnlock() {Unlock();}

	void LockWhen(Condition cond) { Lock();WaitLoop(cond); }
	void ReaderLockWhen(Condition cond) { Lock();WaitLoop(cond); }
	void Await(Condition cond) { WaitLoop(cond); }

private:
	void WaitLoop(Condition cond) {
		signal_at_unlock_=true;
		while(cond.Eval()==false)
			pthread_cond_wait(&cv_,&mu_);
	}

private:
	pthread_mutex_t mu_;
	pthread_cond_t cv_;
	bool signal_at_unlock_;
	friend class CondVar;
};

class MutexLock {  // Scoped Mutex Locker/Unlocker
public:
  	explicit MutexLock(Mutex *mu):mu_(mu) {
    	mu_->Lock();  		
    }
  	~MutexLock() {
    	mu_->Unlock();
  	}
private:
  	Mutex *mu_;
};


class BlockingCounter {
public:
  	explicit BlockingCounter(int initial_count) :
    	count_(initial_count) {}
  	bool DecrementCount() {
    	MutexLock lock(&mu_);
    	count_--;
    	return count_ == 0;
  	}
  	void Wait() {
	    mu_.LockWhen(Condition(&IsZero, &count_));
    	mu_.Unlock();
  	}
private:
  	static bool IsZero(int *arg) { return *arg == 0; }
  	Mutex mu_;
  	int count_;
};

//Wrapper for pthread_create()/pthread_join()
class Thread {
public:
	typedef void *(*worker_t)(void *);
	Thread(worker_t worker,void *arg=NULL,const char *name=NULL)
	:worker_(worker),arg_(arg),name_(name) {}
	Thread(void (*worker)(void),void *arg=NULL,const char *name=NULL)
	:worker_(reinterpret_cast<worker_t>(worker)),arg_(arg),name_(name) {}
	Thread(void (*worker)(void *),void *arg=NULL,const char *name=NULL)
	:worker_(reinterpret_cast<worker_t>(worker)),arg_(arg),name_(name) {}

	~Thread() {
		worker_=NULL;
		arg_=NULL;
	}

	void Start(bool detached=false) {
		pthread_attr_t attr;
		assert(0 == pthread_attr_init(&attr));
		assert(0 == pthread_attr_setstacksize(&attr, 2*1024*1024));
    	assert(0 == pthread_attr_setdetachstate(&attr, detached));
    	assert(0 == pthread_create(&tid_, &attr, (worker_t)ThreadBody, this));
    	assert(0 == pthread_attr_destroy(&attr));
	}

	void Join() {
		assert(0==pthread_join(tid_,NULL));
	}

	pthread_t tid() const {
		return tid_;
	}

private:
	static void ThreadBody(Thread *thread) {
		thread->worker_(thread->arg_);
	}
	pthread_t tid_;
	worker_t worker_;
	void *arg_;
	const char *name_;
};

//wrapper for pthread_cond_t
class CondVar {
public:
	CondVar() { assert(0==pthread_cond_init(&cv_,NULL));}
	~CondVar() { assert(0==pthread_cond_destroy(&cv_)); }

	void Wait(Mutex *mu) { assert(0==pthread_cond_wait(&cv_,&mu->mu_)); }
	void Signal() { assert(0==pthread_cond_signal(&cv_)); }
	void SignalAll() { assert(0==pthread_cond_broadcast(&cv_)); }
private:
	pthread_cond_t cv_;
};

class RWLock {
public:
	RWLock() { assert(0==pthread_rwlock_init(&mu_,NULL)); }
	~RWLock() { assert(0==pthread_rwlock_destroy(&mu_)); }
	void Lock() { assert(0==pthread_rwlock_wrlock(&mu_)); }
	void ReaderLock() { assert(0==pthread_rwlock_rdlock(&mu_)); }
	void Unlock() { assert(0==pthread_rwlock_unlock(&mu_)); }
	void ReaderUnlock() { assert(0==pthread_rwlock_unlock(&mu_)); }
	bool TryLock() {return (0==pthread_rwlock_trywrlock(&mu_));}
	bool ReaderTryLock() {return (0==pthread_rwlock_tryrdlock(&mu_));}
private:
	pthread_rwlock_t mu_;
};

class ReaderLockScoped {
public:
	explicit ReaderLockScoped(RWLock *mu):mu_(mu) { mu_->ReaderLock(); }
	~ReaderLockScoped() { mu_->ReaderUnlock(); }
private:
	RWLock *mu_;
};

class WriterLockScoped {
public:
	explicit WriterLockScoped(RWLock *mu):mu_(mu) { mu_->Lock(); }
	~WriterLockScoped() { mu_->Unlock(); }
private:
	RWLock *mu_;
};

class Barrier {
public:
	Barrier(int n_threads) { assert(0==pthread_barrier_init(&b_,0,n_threads)); }
	~Barrier() { assert(0==pthread_barrier_destroy(&b_)); }
	void Block() {
		pthread_barrier_wait(&b_);
	}
private:
	pthread_barrier_t b_;
};


#endif