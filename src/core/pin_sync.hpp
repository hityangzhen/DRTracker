#ifndef __CORE_PIN_SYNC_HPP
#define __CORE_PIN_SYNC_HPP

#include "pin.H"
#include "core/basictypes.h"
#include "core/sync.h"

//Pin mutex
class PinMutex:public Mutex{
public:
	PinMutex() {PIN_InitLock(&lock_);}
	~PinMutex() {}
	//main threadId is 0
	void Lock() {PIN_GetLock(&lock_,1);}
	void Unlock() {PIN_ReleaseLock(&lock_);}
	Mutex *Clone() {return new PinMutex;}

protected:
	PIN_LOCK lock_;
private:
	DISALLOW_COPY_CONSTRUCTORS(PinMutex);
};


//Pin Semaphore
class PinSemaphore {
public:
	PinSemaphore() {}
	~PinSemaphore() {PIN_SemaphoreFini(&semaphore_);}
	void Init() {PIN_SemaphoreInit(&semaphore_);}
	void Wait() {PIN_SemaphoreWait(&semaphore_);}
	bool IsWaiting() {return !PIN_SemaphoreIsSet(&semaphore_);}
	void TimedWait(unsigned timeout) {PIN_SemaphoreTimedWait(&semaphore_,timeout);}
	void Post() {PIN_SemaphoreSet(&semaphore_);}
private:
	PIN_SEMAPHORE semaphore_;
};

#endif /* __CORE_PIN_SYNC_HPP */