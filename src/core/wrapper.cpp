#include "core/wrapper.hpp"

//Singleton instance for WrapperFactory
WrapperFactory *WrapperFactory::instance_=NULL;

//Register wrappers.
REGISTER_WRAPPER(Malloc);
REGISTER_WRAPPER(Calloc);
REGISTER_WRAPPER(Realloc);
REGISTER_WRAPPER(Free);

REGISTER_WRAPPER(PthreadCreate);
REGISTER_WRAPPER(PthreadJoin);

REGISTER_WRAPPER(PthreadMutexTryLock);
REGISTER_WRAPPER(PthreadMutexLock);
REGISTER_WRAPPER(PthreadMutexUnlock);
REGISTER_WRAPPER(PthreadRwlockTryRdlock);
REGISTER_WRAPPER(PthreadRwlockTryWrlock);
REGISTER_WRAPPER(PthreadRwlockRdlock);
REGISTER_WRAPPER(PthreadRwlockWrlock);
REGISTER_WRAPPER(PthreadRwlockUnlock);
REGISTER_WRAPPER(PthreadCondSignal);
REGISTER_WRAPPER(PthreadCondBroadcast);
REGISTER_WRAPPER(PthreadCondWait);
REGISTER_WRAPPER(PthreadCondTimedwait);
REGISTER_WRAPPER(PthreadBarrierInit);
REGISTER_WRAPPER(PthreadBarrierWait);
REGISTER_WRAPPER(SemInit);
REGISTER_WRAPPER(SemPost);
REGISTER_WRAPPER(SemWait);