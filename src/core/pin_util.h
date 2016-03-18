#ifndef __PIN_UTIL_H
#define __PIN_UTIL_H

#include "pin.H"
#include "core/basictypes.h"

#define CALL_ORDER_BEFORE
#define CALL_ORDER_AFTER

// Global definitions
//Find RTN by function name in an image.
extern RTN FindRTN(IMG img,const std::string &funcName);

//Get the IMG contains the TRACE.
extern IMG GetImgByTrace(TRACE trace);

//Return whethrer the given bbl contains no-stack memory access.
extern bool BBLContainMemOp(BBL bbl);

// Yield the processor to another thread.
extern void Yield();

//Delay execution of the current thread for the specified time interval. 
extern void Sleep(UINT32 milliseconds);

//Create a new tool internal thread in the current process.
extern bool SpawnInternalThread(ROOT_THREAD_FUNC *pThreadFunc,VOID *arg,
	size_t stackSize,PIN_THREAD_UID *pThreadUid);

//Check to see if the current process is about to exit.
extern bool IsProcessExiting();
   	
//Terminate the current thread,This function is intended for threads 
//created by the tool (see PIN_SpawnInternalThread())
extern void ExitThread(INT32 exitCode);
#endif /* __PIN_UTIL_H */

