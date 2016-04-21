#include "tracer/profiler.h"
#include "core/log.h"

namespace tracer
{

Profiler::Profiler():recorder_(NULL)
{}

Profiler::~Profiler()
{
	delete recorder_;
}

void Profiler::HandlePreSetup()
{
	ExecutionControl::HandlePreSetup();

	knob_->RegisterBool("ignore_lib", "whether ignore accesses from common "
		"libraries","0");
	recorder_=new RecorderAnalyzer;
	recorder_->Register();
}

void Profiler::HandlePostSetup()
{
	ExecutionControl::HandlePostSetup();
	recorder_->Setup(CreateMutex());
	AddAnalyzer(recorder_);
}

bool Profiler::HandleIgnoreMemAccess(IMG img)
{
	if(!IMG_Valid(img))
		return true;
	Image *image=sinfo_->FindImage(IMG_Name(img));
	DEBUG_ASSERT(image);
	if(image->IsPthread())
		return true;
	if(knob_->ValueBool("ignore_lib"))
		if(image->IsCommonLib())
			return true;
	return false;
}

} // namespace tracer
