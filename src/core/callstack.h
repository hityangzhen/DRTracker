#ifndef __CORE_CALLSTACK_H
#define __CORE_CALLSTACK_H

/**
 * Track runtime call stacks.
 */

#include <vector>
#include <map>
#include "core/basictypes.h"
#include "core/sync.h"
#include "core/analyzer.h"

//Represent a runtime call stack of a thread
class CallStack {
public:
	typedef uint64 signature_t;
	CallStack():signature_(0) {}
	~CallStack() {}

	signature_t signature() {return signature_;}
	void OnCall(Inst *inst,address_t ret);
	void OnReturn(Inst *inst,address_t target);
	std::string ToString();
protected:
	typedef std::vector<Inst *> InstVec;
	typedef std::vector<address_t> TargetVec;

	signature_t signature_; //current call stack signature
	InstVec instVec;
	TargetVec targetVec;
private:
	DISALLOW_COPY_CONSTRUCTORS(CallStack);
};

//Store the information about runtime call stacks (all threads).
//This class will be used by all analyzers to get call stack informtaion.
class CallStackInfo {
public:
	explicit CallStackInfo(Mutex *lock):internalLock(lock) {}
	~CallStackInfo() {}

	//Return the call stack by its thread id.
	CallStack *GetCallStack(thread_t thdId);
protected:
	typedef std::map<thread_t,CallStack *> StackMap;
	StackMap stackMap;
	Mutex *internalLock;
private:
	DISALLOW_COPY_CONSTRUCTORS(CallStackInfo);
};

//Call-stack-tracker-analyzer  is used to track runtime-call-stacks
//by monitoring every call and return.
class CallStackTracker:public Analyzer {
public:
	explicit CallStackTracker(CallStackInfo *callstackInfo);
	~CallStackTracker() {}

	void Register() {}
	void AfterCall(thread_t currThdId,timestamp_t currThdClk,Inst *inst,
		address_t target,address_t ret);
	void AfterReturn(thread_t currThdId,timestamp_t currThdClk,Inst *inst,
		address_t target);
private:
	DISALLOW_COPY_CONSTRUCTORS(CallStackTracker);
};

#endif