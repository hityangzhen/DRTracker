#include "core/descriptor.h"

Descriptor::Descriptor():
      hook_before_mem_(false),
      hook_after_mem_(false),
      hook_malloc_func_(false),
	  	hook_pthread_func_(false),
	  	hook_atomic_inst_(false),
      hook_call_return_(false),
      hook_main_func_(false),
      track_call_stack_(false),
      track_inst_count_(false),
      skip_stack_access_(true)
{}

//If descriptor initialized ,then hook can not be removed
//only can be added     
void Descriptor::Merge(Descriptor *desc)
{
	hook_before_mem_ = hook_before_mem_ || desc->hook_before_mem_;
  	hook_after_mem_ = hook_after_mem_ || desc->hook_after_mem_;
  	hook_atomic_inst_ = hook_atomic_inst_ || desc->hook_atomic_inst_;
  	hook_pthread_func_ = hook_pthread_func_ || desc->hook_pthread_func_;
  	hook_malloc_func_ = hook_malloc_func_ || desc->hook_malloc_func_;
  	hook_main_func_ = hook_main_func_ || desc->hook_main_func_;
  	hook_call_return_ = hook_call_return_ || desc->hook_call_return_;
  	track_inst_count_ = track_inst_count_ || desc->track_inst_count_;
  	track_call_stack_ = track_call_stack_ || desc->track_call_stack_;
  	skip_stack_access_ = skip_stack_access_ && desc->skip_stack_access_;
}

