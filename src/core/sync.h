#ifndef __CORE_SYNC_H
#define __CORE_SYNC_H

/**
 * Define synchronizations.
 */
#include <semaphore.h>
#include "core/basictypes.h"


//Mutex interface
class Mutex {
public:
	Mutex(){}
	virtual ~Mutex(){}

	virtual void Lock()=0;
	virtual void Unlock()=0;
	virtual Mutex *Clone()=0;
private:
	DISALLOW_COPY_CONSTRUCTORS(Mutex);
};

//define semphore interface
class Semaphore {
public:
	Semaphore() {}
	virtual ~Semaphore() {}

	virtual int Init(unsigned int value)=0;
	virtual int Wait()=0;
	virtual int TimedWait(const struct timespec *to)=0;
	virtual int Post()=0;
private:
	DISALLOW_COPY_CONSTRUCTORS(Semaphore);
};

// Define the null mutex (used in single threaded mode)
class NullMutex : public Mutex {
public:
  	NullMutex() {}
  	~NullMutex() {}

  	void Lock() {}
  	void Unlock() {}
  	Mutex *Clone() { return new NullMutex; }
private:
  	DISALLOW_COPY_CONSTRUCTORS(NullMutex);
};

//Read-Write mutex interface
class RWMutex {
public:
	RWMutex(){};
	virtual ~RWMutex(){}

	virtual void LockRead()=0;
	virtual void UnlockRead()=0;
	virtual void LockWrite()=0;
	virtual void UnlockWrite()=0;
	virtual RWMutex *Clone()=0;

private:
	DISALLOW_COPY_CONSTRUCTORS(RWMutex);
};

//Local scoped lock
class ScopedLock {
public:
	explicit ScopedLock(Mutex *mutex):mutex_(mutex) {
		mutex_->Lock();
		locked_=true;
	}

	ScopedLock(Mutex *mutex,bool initiallyLocking):mutex_(mutex) {
		if(initiallyLocking) {
			mutex_->Lock();
			locked_=true;		
		}
	}

	~ScopedLock() {
		if(locked_)
			mutex_->Unlock();
	}
private:
	Mutex *mutex_;
	bool locked_;
	DISALLOW_COPY_CONSTRUCTORS(ScopedLock);
};

//define the semphore implemented by the linux
class SysSemaphore:public Semaphore {
public:
	SysSemaphore() {}
	explicit SysSemaphore(unsigned int value) { Init(value); }
	~SysSemaphore()  {sem_destroy(&sem_);}

	int Init(unsigned int value) { return sem_init(&sem_,0,value); }
	int Wait() { return sem_wait(&sem_); }
	int TimedWait(const struct timespec *to) { return sem_timedwait(&sem_,to); }
	int Post() { return sem_post(&sem_); }
private:
	sem_t sem_;

	DISALLOW_COPY_CONSTRUCTORS(SysSemaphore);
};

#endif /* __CORE_SYNC_H */