#ifndef __CORE_DESCRIPTOR_H
#define __CORE_DESCRIPTOR_H

/**
 * Define hook function descriptions for instrumenting the application program. 
 */
#include "core/basictypes.h"

class Descriptor {
public:
	Descriptor();
	~Descriptor() {}

	void Merge(Descriptor *desc);
	bool HookMem() { return hook_before_mem_ || hook_after_mem_; }
  	bool HookBeforeMem() { return hook_before_mem_; }
  	bool HookAfterMem() { return hook_after_mem_; }
	bool HookMallocFunc() {return hook_malloc_func_;}
	bool HookPthreadFunc() {return hook_pthread_func_;}
	bool HookAtomicInst() {return hook_atomic_inst_;}
	bool HookCallReturn() {return hook_call_return_;}
	bool HookMainFunc() { return hook_main_func_; }
	bool TrackCallStack() { return track_call_stack_; }
	bool TrackInstCount() { return track_inst_count_; }
	bool SkipStackAccess() { return skip_stack_access_; }
	//
	void SetHookBeforeMem() { hook_before_mem_= true; }
  	void SetHookAfterMem() { hook_after_mem_ = true; }
	void SetHookMallocFunc() { hook_malloc_func_=true; }
	void SetHookPthreadFunc() {hook_pthread_func_=true;}
	void SetHookMainFunc() { hook_main_func_ = true; }
	void SetHookAtomicInst() {hook_atomic_inst_=true;}
	void SetHookCallReturn() {hook_call_return_=true;}
	void SetTrackCallStack() { track_call_stack_ = true; }
	void SetTrackInstCount() { track_inst_count_=true; }
	void SetNoSkipStackAccess() {  skip_stack_access_=false;}

protected:
	bool hook_before_mem_;
	bool hook_after_mem_;
	bool hook_malloc_func_;
	bool hook_pthread_func_;
	bool hook_atomic_inst_;
	bool hook_call_return_;
	bool hook_main_func_;
	bool track_call_stack_;
	bool track_inst_count_;
	bool skip_stack_access_;

private:
	DISALLOW_COPY_CONSTRUCTORS(Descriptor);
};

#endif /* __CORE_DESCRIPTOR_H */