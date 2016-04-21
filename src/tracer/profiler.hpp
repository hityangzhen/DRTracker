/**
  * @file tracer/profiler.hpp
  * Define tracer online profiler.
  */
#ifndef __TRACER_PROFILER_HPP
#define __TRACER_PROFILER_HPP

#include "core/execution_control.hpp"
#include "tracer/recorder.h"

namespace tracer
{
class TraceLog;

class Profiler:public ExecutionControl {
public:
	Profiler();
	~Profiler();
private:
	void HandlePreSetup();
	void HandlePostSetup();
	bool HandleIgnoreMemeAccess(IMG img);

	RecorderAnalyzer *recorder_;

	DISALLOW_COPY_CONSTRUCTORS(Profiler);
};

} // namespace tracer
 
#endif // __TRACER_PROFILER_HPP