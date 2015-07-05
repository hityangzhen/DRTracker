#include "core/debug_analyzer.h"


void DebugAnalyzer::Register() 
{
	knob_->RegisterBool("enable_debug", "whether enable the debug analyzer", "0");
  knob_->RegisterBool("debug_mem", "whether debug mem accesses", "0");
  knob_->RegisterBool("debug_atomic", "whether debug atomic inst", "0");
  knob_->RegisterBool("debug_main", "whether debug main functions", "0");
  knob_->RegisterBool("debug_call_return", "whether debug calls and returns", "0");
  knob_->RegisterBool("debug_pthread", "whether debug pthread functions", "0");
  knob_->RegisterBool("debug_malloc", "whether debug malloc functions", "0");
  knob_->RegisterBool("debug_track_callstack", "whether track runtime call stack", "0");
}

bool DebugAnalyzer::Enabled()
{
	return knob_->ValueBool("enable_debug");
}

void DebugAnalyzer::Setup()
{
	if (knob_->ValueBool("debug_mem"))
    desc_.SetHookBeforeMem();
  if (knob_->ValueBool("debug_atomic"))
    desc_.SetHookAtomicInst();
  if (knob_->ValueBool("debug_main"))
    desc_.SetHookMainFunc();
  if (knob_->ValueBool("debug_call_return"))
    desc_.SetHookCallReturn();
  if (knob_->ValueBool("debug_pthread"))
    desc_.SetHookPthreadFunc();
  if (knob_->ValueBool("debug_malloc"))
    desc_.SetHookMallocFunc();
  if (knob_->ValueBool("debug_track_callstack"))
    desc_.SetTrackCallStack();
}