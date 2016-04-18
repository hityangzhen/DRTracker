#include "tracer/recorder.h"

namespace tracer
{

RecorderAnalyzer::RecorderAnalyzer():internal_lock_(NULL),trace_log_(NULL)
{}

void RecorderAnalyzer::Register() 
{
	knob_->RegisterBool("enable_recorder", "whether enable the recorder analyzer", "0");
	knob_->RegisterStr("trace_log_path", "the trace log path", "trace-log");
	knob_->RegisterBool("trace_mem", "whether record memory accesses", "1");
	knob_->RegisterBool("trace_atomic", "whether record atomic instructions", "1");
	knob_->RegisterBool("trace_main", "whether record thread main functions", "1");
	knob_->RegisterBool("trace_pthread", "whether record pthread functions", "1");
	knob_->RegisterBool("trace_malloc", "whether record memory allocation function", "1");
	knob_->RegisterBool("trace_track_clk", "whether track per thread clock", "1");
}

bool RecorderAnalyzer::Enabled() 
{
	return knob_->ValueBool("enable_recorder");
}

void RecorderAnalyzer::Setup(Mutex *lock)
{
	internal_lock_=lock;
	// setup analyzer descriptor
	if (knob_->ValueBool("trace_mem"))
		desc_.SetHookBeforeMem();
	if (knob_->ValueBool("trace_atomic"))
		desc_.SetHookAtomicInst();
	if (knob_->ValueBool("trace_main"))
		desc_.SetHookMainFunc();
	if (knob_->ValueBool("trace_pthread"))
		desc_.SetHookPthreadFunc();
	if (knob_->ValueBool("trace_malloc"))
		desc_.SetHookMallocFunc();
	if (knob_->ValueBool("trace_track_clk"))
		desc_.SetTrackInstCount();

	// create trace log and open it
	trace_log_ = new TraceLog(knob_->ValueStr("trace_log_path"));
}

} //namespace tracer