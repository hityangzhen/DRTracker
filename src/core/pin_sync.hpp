#ifndef __CORE_PIN_SYNC_H
#define __CORE_PIN_SYNC_H

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

#endif /* __CORE_PIN_SYNC_H */