#include "core/callstack.h"
#include "core/log.h"

void CallStack::OnCall(Inst *inst,address_t ret)
{
	instVec.push_back(inst);
	targetVec.push_back(ret);

	//signature must be unique,here we use inst id instead 
	//pointer value

	signature_ += (signature_t)inst->id();
	//debug format print
	DEBUG_FMT_PRINT_SAFE("(%s)\n",ToString().c_str());
}


void CallStack::OnReturn(Inst *inst,address_t target)
{
	DEBUG_ASSERT(instVec.size()==targetVec.size());

	assert(!instVec.empty());

	//Backward search matching target address,find the first one that
	//matches and remove the etires after it.
	bool found=false;
	int size=(int)instVec.size();
	int newsize=size;
	signature_t newsignature=signature_;
	for(int i=size-1;i>-1;i--) {
		Inst *inst=instVec[i];
		address_t addr=targetVec[i];

		newsize-=1;
		newsignature-=inst->id();

		if(addr==target) {
			found=true;
			break;
		}
	}

	if(found) {
		instVec.erase(instVec.begin()+newsize,instVec.end());
		targetVec.erase(targetVec.begin()+newsize,targetVec.end());
		signature_=newsignature;
	}

	DEBUG_FMT_PRINT_SAFE("(%s)\n",ToString().c_str());
}

std::string CallStack::ToString()
{
	std::stringstream ss;
	ss<<std::hex;
	size_t size=instVec.size();
	for(size_t i=0;i<size;i++) {
		ss<<"<"<<instVec[i]->id()<<" ";
		ss<<"0x"<<targetVec[i]<<">";
		if(i!=size-1)
			ss<<" ";
	}
	return ss.str();
}

CallStack *CallStackInfo::GetCallStack(thread_t thdId) 
{
	//automaticly release lock
	ScopedLock lock(internalLock);

	//If current thread have call stack
	StackMap::iterator it=stackMap.find(thdId);
	if(it==stackMap.end()) {
		CallStack *callstack=new CallStack;
		stackMap[thdId]=callstack;
		return callstack;
	}

	return it->second;
}

CallStackTracker::CallStackTracker(CallStackInfo *callStackInfo)
{
	DEBUG_ASSERT(callStackInfo);
	//super calss
	setCallStackInfo(callStackInfo);
	//Need to monitor calls and return
	desc_.SetHookCallReturn();
}

void CallStackTracker::AfterCall(thread_t currThdId,timestamp_t currThdClk,
	Inst *inst,address_t target,address_t ret) 
{
	CallStack *callStack=callStackInfo->GetCallStack(currThdId);
	callStack->OnCall(inst,ret);
}

void CallStackTracker::AfterReturn(thread_t currThdId,timestamp_t currThdClk,
	Inst *inst,address_t target) 
{
	CallStack *callStack=callStackInfo->GetCallStack(currThdId);
	callStack->OnReturn(inst,target);	
}