/**
	* @file tracer/loader.h
	* Define the tracer loader. Load the trace long and replay the behavior
	* of the program. At the same time, loader distributes the events to the
	* list of analyzers. 
	* Anyone needs to extend the analysis should inherit the Analyzer and 
	* adds the new analyzer into the list of the analyzers.
	*/

#ifndef __TRACER_LOADER_H
#define __TRACER_LOADER_H

#include <list>
#include "core/basictypes.h"
#include "core/sync.h"
#include "core/log.h"
#include "core/offline_tool.h"
#include "core/descriptor.h"
#include "core/analyzer.h"
#include "tracer/log.h"

namespace tracer
{

class Loader:public OfflineTool {
public:
	Loader();
	virtual ~Loader();

protected:
	typedef std::list<Analyzer *> AnalyzerContainer;

	virtual void HandlePreSetup();
	virtual void HandlePostSetup();
	virtual void HandleStart();
	virtual void HandleProgramStart(LogEntry *e);
	virtual void HandleProgramExit(LogEntry *e);
	virtual void HandleImageLoad(LogEntry *e);
	virtual void HandleImageUnload(LogEntry *e);
	virtual void HandleThreadStart(LogEntry *e);
	virtual void HandleThreadExit(LogEntry *e);
	virtual void HandleMain(LogEntry *e);
	virtual void HandleThreadMain(LogEntry *e);
	virtual void HandleBeforeMemRead(LogEntry *e);
	virtual void HandleAfterMemRead(LogEntry *e);
	virtual void HandleBeforeMemWrite(LogEntry *e);
	virtual void HandleAfterMemWrite(LogEntry *e);
	virtual void HandleBeforeAtomicInst(LogEntry *e);
	virtual void HandleAfterAtomicInst(LogEntry *e);
	virtual void HandleBeforePthreadCreate(LogEntry *e);
	virtual void HandleAfterPthreadCreate(LogEntry *e);
	virtual void HandleBeforePthreadJoin(LogEntry *e);
	virtual void HandleAfterPthreadJoin(LogEntry *e);
	virtual void HandleBeforePthreadMutexTryLock(LogEntry *e);
	virtual void HandleAfterPthreadMutexTryLock(LogEntry *e);
	virtual void HandleBeforePthreadMutexLock(LogEntry *e);
	virtual void HandleAfterPthreadMutexLock(LogEntry *e);
	virtual void HandleBeforePthreadMutexUnlock(LogEntry *e);
	virtual void HandleAfterPthreadMutexUnlock(LogEntry *e);
	virtual void HandleBeforePthreadRwlockTryRdlock(LogEntry *e);
	virtual void HandleAfterPthreadRwlockTryRdlock(LogEntry *e);
	virtual void HandleBeforePthreadRwlockRdlock(LogEntry *e);
	virtual void HandleAfterPthreadRwlockRdlock(LogEntry *e);
	virtual void HandleBeforePthreadRwlockTryWrlock(LogEntry *e);
	virtual void HandleAfterPthreadRwlockTryWrlock(LogEntry *e);
	virtual void HandleBeforePthreadRwlockWrlock(LogEntry *e);
	virtual void HandleAfterPthreadRwlockWrlock(LogEntry *e);
	virtual void HandleBeforePthreadRwlockUnlock(LogEntry *e);
	virtual void HandleAfterPthreadRwlockUnlock(LogEntry *e);
	virtual void HandleBeforePthreadCondSignal(LogEntry *e);
	virtual void HandleAfterPthreadCondSignal(LogEntry *e);
	virtual void HandleBeforePthreadCondBroadcast(LogEntry *e);
	virtual void HandleAfterPthreadCondBroadcast(LogEntry *e);
	virtual void HandleBeforePthreadCondWait(LogEntry *e);
	virtual void HandleAfterPthreadCondWait(LogEntry *e);
	virtual void HandleBeforePthreadCondTimedwait(LogEntry *e);
	virtual void HandleAfterPthreadCondTimedwait(LogEntry *e);
	virtual void HandleBeforePthreadBarrierInit(LogEntry *e);
	virtual void HandleAfterPthreadBarrierInit(LogEntry *e);
	virtual void HandleBeforePthreadBarrierWait(LogEntry *e);
	virtual void HandleAfterPthreadBarrierWait(LogEntry *e);
	virtual void HandleBeforeSemInit(LogEntry *e);
	virtual void HandleAfterSemInit(LogEntry *e);
	virtual void HandleBeforeSemPost(LogEntry *e);
	virtual void HandleAfterSemPost(LogEntry *e);
	virtual void HandleBeforeSemWait(LogEntry *e);
	virtual void HandleAfterSemWait(LogEntry *e);
	virtual void HandleBeforeMalloc(LogEntry *e);
	virtual void HandleAfterMalloc(LogEntry *e);
	virtual void HandleBeforeCalloc(LogEntry *e);
	virtual void HandleAfterCalloc(LogEntry *e);
	virtual void HandleBeforeRealloc(LogEntry *e);
	virtual void HandleAfterRealloc(LogEntry *e);
	virtual void HandleBeforeFree(LogEntry *e);
	virtual void HandleAfterFree(LogEntry *e);
	virtual void HandleBeforeValloc(LogEntry *e);
	virtual void HandleAfterValloc(LogEntry *e);

	void EventLoop();
	void HandleEvent(LogEntry *e);
	void AddAnalyzer(Analyzer *analyzer);

	TraceLog *trace_log_;
	AnalyzerContainer analyzers_;
	Descriptor desc_;
	DebugAnalyzer *debug_analyzer_;
private:
	DISALLOW_COPY_CONSTRUCTORS(Loader);
};

} // namespace tracer

#endif // __TRACER_LOADER_H