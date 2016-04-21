#include "tracer/loader.h"
#include "core/cmdline_knob.h"
#include "core/debug_analyzer.h"

#define CALL_ANALYSIS_FUNC(func,...)										\
	for(AnalyzerContainer::iterator iter=analyzers_.begin();				\
		iter!=analyzers_.end();++iter) {									\
		(*iter)->func(__VA_ARGS__);											\
	}

#define CALL_ANALYSIS_FUNC2(type,func,...)									\
	for(AnalyzerContainer::iterator iter=analyzers_.begin();				\
		iter!=analyzers_.end();++iter) {									\
		if((*iter)->desc()->Hook##type())									\
			(*iter)->func(__VA_ARGS__);										\
	}

namespace tracer
{

Loader::Loader():tracer_log_(NULL),debug_analyzer_(NULL)
{}

Loader::~Loader()
{
	delete debug_analyzer_;
	delete tracer_log_;
}

void Loader::HandlePreSetup() 
{
	OfflineTool::HandlePreSetup();
	knob_->RegisterStr("tracer_log_path","the trace log file","trace-log");
	debug_analyzer_=new DebugAnalyzerl;
	debug_analyzer_->Register();
}

void Loader::HandlePostSetup()
{
	OfflineTool::HandlePostSetup();
	// load trace log
	tracer_log_=new TraceLog(knob_->ValueStr("tracer_log_path"));

	if(debug_analyzer_->Enabled()) {
		// add debug analyzer if necessary
		debug_analyzer_->Setup();
		AddAnalyzer(debug_analyzer_);
	}
}

void Loader::HandleStart()
{
	tracer_log_->OpenForRead();
	EventLoop();
	tracer_log_->CloseForRead();
}


inline void Loader::EventLoop()
{
	while(tracer_log_->HasNextEntry()) {
		LogEntry entry=tracer_log_->NextEntry();
		HandleEvent(&entry);
	}
}

inline void Loader::AddAnalyzer(Analyzer *analyzer)
{
	analyzers_.push_back(analyzer);
	desc_.Merge(analyzer->desc());
}

void Loader::HandleEvent(LogEntry *e)
{
	switch(e->type()) {
		case LOG_ENTRY_PROGRAM_START:
	      	HandleProgramStart(e);
	      	break;
	    case LOG_ENTRY_PROGRAM_EXIT:
	      	HandleProgramExit(e);
	      	break;
	    case LOG_ENTRY_IMAGE_LOAD:
	      	HandleImageLoad(e);
	      	break;
	    case LOG_ENTRY_IMAGE_UNLOAD:
	      	HandleImageUnload(e);
	      	break;
	    case LOG_ENTRY_THREAD_START:
	      	HandleThreadStart(e);
	      	break;
	    case LOG_ENTRY_THREAD_EXIT:
	     	HandleThreadExit(e);
	      	break;
	    case LOG_ENTRY_MAIN:
	     	HandleMain(e);
	      	break;
	    case LOG_ENTRY_THREAD_MAIN:
	      	HandleThreadMain(e);
	      	break;
	    case LOG_ENTRY_BEFORE_MEM_READ:
	      	HandleBeforeMemRead(e);
	      	break;
	    case LOG_ENTRY_AFTER_MEM_READ:
	      	HandleAfterMemRead(e);
	      	break;
	    case LOG_ENTRY_BEFORE_MEM_WRITE:
	      	HandleBeforeMemWrite(e);
	      	break;
	    case LOG_ENTRY_AFTER_MEM_WRITE:
	      	HandleAfterMemWrite(e);
	      	break;
	    case LOG_ENTRY_BEFORE_ATOMIC_INST:
	      	HandleBeforeAtomicInst(e);
	      	break;
	    case LOG_ENTRY_AFTER_ATOMIC_INST:
	      	HandleAfterAtomicInst(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_CREATE:
	      	HandleBeforePthreadCreate(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_CREATE:
	      	HandleAfterPthreadCreate(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_JOIN:
	      	HandleBeforePthreadJoin(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_JOIN:
	      	HandleAfterPthreadJoin(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_MUTEX_TRYLOCK:
	      	HandleBeforePthreadMutexTryLock(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_MUTEX_TRYLOCK:
	      	HandleAfterPthreadMutexTryLock(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_MUTEX_LOCK:
	      	HandleBeforePthreadMutexLock(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_MUTEX_LOCK:
	      	HandleAfterPthreadMutexLock(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_MUTEX_UNLOCK:
	      	HandleBeforePthreadMutexUnlock(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_MUTEX_UNLOCK:
	      	HandleAfterPthreadMutexUnlock(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_RWLOCK_TRYRDLOCK:
	    	HandleBeforePthreadRwlockTryRdlock(e);
	    	break;
	    case LOG_ENTRY_AFTER_PTHREAD_RWLOCK_TRYRDLOCK:
	    	HandleAfterPthreadRwlockTryRdlock(e);
	    	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_RWLOCK_RDLOCK:
	    	HandleBeforePthreadRwlockRdlock(e);
	    	break;
	    case LOG_ENTRY_AFTER_PTHREAD_RWLOCK_RDLOCK:
	    	HandleAfterPthreadRwlockRdlock(e);
	    	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_RWLOCK_TRYWRLOCK:
	    	HandleBeforePthreadRwlockTryWrlock(e);
	    	break;
	    case LOG_ENTRY_AFTER_PTHREAD_RWLOCK_TRYWRLOCK:
	    	HandleAfterPthreadRwlockTryWrlock(e);
	    	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_RWLOCK_WRLOCK:
	    	HandleBeforePthreadRwlockWrlock(e);
	    	break;
	    case LOG_ENTRY_AFTER_PTHREAD_RWLOCK_WRLOCK:
	    	HandleAfterPthreadRwlockWrlock(e);
	    	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_RWLOCK_UNLOCK:
	    	HandleBeforePthreadRwlockUnlock(e);
	    	break;
	    case LOG_ENTRY_AFTER_PTHREAD_RWLOCK_UNLOCK:
	    	HandleAfterPthreadRwlockUnlock(e);
	    	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_COND_SIGNAL:
	      	HandleBeforePthreadCondSignal(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_COND_SIGNAL:
	      	HandleAfterPthreadCondSignal(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_COND_BROADCAST:
	      	HandleBeforePthreadCondBroadcast(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_COND_BROADCAST:
	      	HandleAfterPthreadCondBroadcast(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_COND_WAIT:
	      	HandleBeforePthreadCondWait(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_COND_WAIT:
	      	HandleAfterPthreadCondWait(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_COND_TIMEDWAIT:
	      	HandleBeforePthreadCondTimedwait(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_COND_TIMEDWAIT:
	      	HandleAfterPthreadCondTimedwait(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_BARRIER_INIT:
	      	HandleBeforePthreadBarrierInit(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_BARRIER_INIT:
	      	HandleAfterPthreadBarrierInit(e);
	      	break;
	    case LOG_ENTRY_BEFORE_PTHREAD_BARRIER_WAIT:
	      	HandleBeforePthreadBarrierWait(e);
	      	break;
	    case LOG_ENTRY_AFTER_PTHREAD_BARRIER_WAIT:
	      	HandleAfterPthreadBarrierWait(e);
	      	break;
	    case LOG_ENTRY_BEFORE_SEM_INIT:
	    	HandleBeforeSemInit(e);
	    	break;
	   	case LOG_ENTRY_AFTER_SEM_INIT:
	    	HandleAfterSemInit(e);
	    	break;
	   	case LOG_ENTRY_BEFORE_SEM_POST:
	    	HandleBeforeSemPost(e);
	    	break;
	    case LOG_ENTRY_AFTER_SEM_POST:
	    	HandleAfterSemPost(e);
	    	break;
	    case LOG_ENTRY_BEFORE_SEM_WAIT:
	    	HandleBeforeSemWait(e);
	    	break;
	    case LOG_ENTRY_AFTER_SEM_WAIT:
	    	HandleAfterSemWait(e);
	    	break;
	    case LOG_ENTRY_BEFORE_MALLOC:
	      	HandleBeforeMalloc(e);
	      	break;
	    case LOG_ENTRY_AFTER_MALLOC:
	      	HandleAfterMalloc(e);
	      	break;
	    case LOG_ENTRY_BEFORE_CALLOC:
	      	HandleBeforeCalloc(e);
	      	break;
	    case LOG_ENTRY_AFTER_CALLOC:
	      	HandleAfterCalloc(e);
	      	break;
	    case LOG_ENTRY_BEFORE_REALLOC:
	      	HandleBeforeRealloc(e);
	      	break;
	    case LOG_ENTRY_AFTER_REALLOC:
	      	HandleAfterRealloc(e);
	      	break;
	    case LOG_ENTRY_BEFORE_FREE:
	      	HandleBeforeFree(e);
	      	break;
	    case LOG_ENTRY_AFTER_FREE:
	      	HandleAfterFree(e);
	      	break;
	    case LOG_ENTRY_BEFORE_VALLOC:
	      	HandleBeforeValloc(e);
	      	break;
	    case LOG_ENTRY_AFTER_VALLOC:
	      	HandleAfterValloc(e);
	      	break;
	    default:
	      	DEBUG_FMT_PRINT_SAFE("e->type() = %d\n", e->type());
	      	DEBUG_ASSERT(0); // impossible
	      	break;
	}
}

void Loader::HandleProgramStart(LogEntry *e)
{
	CALL_ANALYSIS_FUNC(ProgramStart);
}

void Loader::HandleProgramExit(LogEntry *e)
{
	CALL_ANALYSIS_FUNC(ProgramExit);
}

void Loader::HandleImageLoad(LogEntry *e)
{
	image_t image_id=(image_t)e->arg(0);
	Image *image=sinfo_->FindImage(image_id);
	DEBUG_ASSERT(image);
	address_t low_addr = e->arg(1);
  	address_t high_addr = e->arg(2);
  	address_t data_start = e->arg(3);
  	size_t data_size = e->arg(4);
  	address_t bss_start = e->arg(5);
  	size_t bss_size = e->arg(6);
  	CALL_ANALYSIS_FUNC(ImageLoad, image, low_addr, high_addr,
  		data_start, data_size, bss_start, bss_size);
}

void Loader::HandleImageUnload(LogEntry *e)
{
	image_t image_id=(image_t)e->arg(0);
	Image *image=sinfo_->FindImage(image_id);
	DEBUG_ASSERT(image);
	address_t low_addr = e->arg(1);
  	address_t high_addr = e->arg(2);
  	address_t data_start = e->arg(3);
  	size_t data_size = e->arg(4);
  	address_t bss_start = e->arg(5);
  	size_t bss_size = e->arg(6);
  	CALL_ANALYSIS_FUNC(ImageUnload, image, low_addr, high_addr,
  		data_start, data_size, bss_start, bss_size);
}

void Loader::HandleThreadStart(LogEntry *e) {
  thread_t self = e->thd_id();
  thread_t parent = e->arg(0);
  CALL_ANALYSIS_FUNC(ThreadStart, self, parent);
}

void Loader::HandleThreadExit(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  CALL_ANALYSIS_FUNC(ThreadExit, self, curr_thd_clk);
}

void Loader::HandleMain(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  CALL_ANALYSIS_FUNC2(MainFunc, Main, self, curr_thd_clk);
}

void Loader::HandleThreadMain(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  CALL_ANALYSIS_FUNC2(MainFunc, ThreadMain, self, curr_thd_clk);
}

void Loader::HandleBeforeMemRead(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(BeforeMem, BeforeMemRead, self, curr_thd_clk,
  	inst, addr, size);
}

void Loader::HandleAfterMemRead(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(AfterMem, AfterMemRead, self, curr_thd_clk,
  	inst, addr, size);
}

void Loader::HandleBeforeMemWrite(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(BeforeMem, BeforeMemWrite, self, curr_thd_clk,
  	inst, addr, size);
}

void Loader::HandleAfterMemWrite(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(AfterMem, AfterMemWrite, self, curr_thd_clk,
  	inst, addr, size);
}

void Loader::HandleBeforeAtomicInst(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  std::string type = e->str_arg(0);
  address_t addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(AtomicInst, BeforeAtomicInst, self, curr_thd_clk,
  	inst, type, addr);
}

void Loader::HandleAfterAtomicInst(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  std::string type = e->str_arg(0);
  address_t addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(AtomicInst, AfterAtomicInst, self, curr_thd_clk,
  	inst, type, addr);
}

void Loader::HandleBeforePthreadCreate(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadCreate, self,
  	curr_thd_clk, inst);
}

void Loader::HandleAfterPthreadCreate(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  thread_t child_thd_id = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadCreate, self,
  	curr_thd_clk, inst, child_thd_id);
}

void Loader::HandleBeforePthreadJoin(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  thread_t child_thd_id = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadJoin, self,
  	curr_thd_clk, inst, child_thd_id);
}

void Loader::HandleAfterPthreadJoin(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  thread_t child_thd_id = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadJoin, self,
  	curr_thd_clk, inst, child_thd_id);
}

void Loader::HandleBeforePthreadMutexTryLock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadMutexTryLock, self,
  	curr_thd_clk, inst, mutex_addr);
}

void Loader::HandleAfterPthreadMutexTryLock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  int ret_val = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadMutexTryLock, self,
  	curr_thd_clk, inst, mutex_addr, ret_val);
}

void Loader::HandleBeforePthreadMutexLock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadMutexLock, self,
  	curr_thd_clk, inst, mutex_addr);
}

void Loader::HandleAfterPthreadMutexLock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadMutexLock, self,
  	curr_thd_clk, inst, mutex_addr);
}

void Loader::HandleBeforePthreadMutexUnlock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadMutexUnlock, self,
  	curr_thd_clk, inst, mutex_addr);
}

void Loader::HandleAfterPthreadMutexUnlock(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t mutex_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadMutexUnlock, self,
  	curr_thd_clk, inst, mutex_addr);
}

void Loader::HandleBeforePthreadRwlockTryRdlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadRwlockTryRdlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleAfterPthreadRwlockTryRdlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	int ret_val = e->arg(1);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadRwlockTryRdlock, self,
  	curr_thd_clk, inst, rwlock_addr,ret_val);
}

void Loader::HandleBeforePthreadRwlockRdlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadRwlockRdlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleAfterPthreadRwlockRdlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadRwlockRdlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleBeforePthreadRwlockTryWrlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadRwlockTryWrlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleAfterPthreadRwlockTryWrlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	int ret_val=e->arg(1);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadRwlockTryWrlock, self,
  	curr_thd_clk, inst, rwlock_addr,ret_val);
}

void Loader::HandleBeforePthreadRwlockWrlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadRwlockWrlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleAfterPthreadRwlockWrlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadRwlockWrlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleBeforePthreadRwlockUnlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadRwlockUnlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleAfterPthreadRwlockUnlock(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t rwlock_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadRwlockUnlock, self,
  	curr_thd_clk, inst, rwlock_addr);
}

void Loader::HandleBeforePthreadCondSignal(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadCondSignal, self,
  	curr_thd_clk, inst, cond_addr);
}

void Loader::HandleAfterPthreadCondSignal(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadCondSignal, self,
  	curr_thd_clk, inst, cond_addr);
}

void Loader::HandleBeforePthreadCondBroadcast(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadCondBroadcast, self,
  	curr_thd_clk, inst, cond_addr);
}

void Loader::HandleAfterPthreadCondBroadcast(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadCondBroadcast, self,
  	curr_thd_clk, inst, cond_addr);
}

void Loader::HandleBeforePthreadCondWait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  address_t mutex_addr = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadCondWait, self,
  	curr_thd_clk, inst, cond_addr, mutex_addr);
}

void Loader::HandleAfterPthreadCondWait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  address_t mutex_addr = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadCondWait, self,
  	curr_thd_clk, inst, cond_addr, mutex_addr);
}

void Loader::HandleBeforePthreadCondTimedwait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  address_t mutex_addr = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadCondTimedwait, self,
  	curr_thd_clk, inst, cond_addr, mutex_addr);
}

void Loader::HandleAfterPthreadCondTimedwait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t cond_addr = e->arg(0);
  address_t mutex_addr = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadCondTimedwait, self,
  	curr_thd_clk, inst, cond_addr, mutex_addr);
}

void Loader::HandleBeforePthreadBarrierInit(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  unsigned int count = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadBarrierInit, self,
  	curr_thd_clk, inst, addr, count);
}

void Loader::HandleAfterPthreadBarrierInit(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t addr = e->arg(0);
  unsigned int count = e->arg(1);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadBarrierInit, self,
  	curr_thd_clk, inst, addr, count);
}

void Loader::HandleBeforePthreadBarrierWait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t barrier_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, BeforePthreadBarrierWait, self,
  	curr_thd_clk, inst, barrier_addr);
}

void Loader::HandleAfterPthreadBarrierWait(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t barrier_addr = e->arg(0);
  CALL_ANALYSIS_FUNC2(PthreadFunc, AfterPthreadBarrierWait, self,
  	curr_thd_clk, inst, barrier_addr);
}

void Loader::HandleBeforeSemInit(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	unsigned int value=e->arg(1);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforeSemInit, self,curr_thd_clk, inst, 
  		sem_addr,value);
}

void Loader::HandleAfterSemInit(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	unsigned int value=e->arg(1);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterSemInit, self,curr_thd_clk, inst, 
  		sem_addr,value);
}

void Loader::HandleBeforeSemPost(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforeSemPost, self,curr_thd_clk, inst, 
  		sem_addr);
}

void Loader::HandleAfterSemPost(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterSemPost, self,curr_thd_clk, inst, 
  		sem_addr);
}

void Loader::HandleBeforeSemWait(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, BeforeSemWait, self,curr_thd_clk, inst, 
  		sem_addr);
}

void Loader::HandleAfterSemWait(LogEntry *e)
{
	thread_t self = e->thd_id();
  	timestamp_t curr_thd_clk = e->thd_clk();
  	Inst *inst = sinfo_->FindInst(e->inst_id());
  	DEBUG_ASSERT(inst);
  	address_t sem_addr = e->arg(0);
  	CALL_ANALYSIS_FUNC2(PthreadFunc, AfterSemWait, self,curr_thd_clk, inst, 
  		sem_addr);
}

void Loader::HandleBeforeMalloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t size = e->arg(0);
  CALL_ANALYSIS_FUNC2(MallocFunc, BeforeMalloc, self,curr_thd_clk, 
  	inst, size);
}

void Loader::HandleAfterMalloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t size = e->arg(0);
  address_t ret_val = e->arg(1);
  CALL_ANALYSIS_FUNC2(MallocFunc, AfterMalloc, self,curr_thd_clk, inst,
  	size, ret_val);
}

void Loader::HandleBeforeCalloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t nmemb = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(MallocFunc, BeforeCalloc, self,curr_thd_clk, inst, 
  	nmemb, size);
}

void Loader::HandleAfterCalloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t nmemb = e->arg(0);
  size_t size = e->arg(1);
  address_t ret_val = e->arg(2);
  CALL_ANALYSIS_FUNC2(MallocFunc, AfterCalloc, self,curr_thd_clk, inst, 
  	nmemb, size, ret_val);
}

void Loader::HandleBeforeRealloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t ptr = e->arg(0);
  size_t size = e->arg(1);
  CALL_ANALYSIS_FUNC2(MallocFunc, BeforeRealloc, self,curr_thd_clk, inst, 
  	ptr, size);
}

void Loader::HandleAfterRealloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t ptr = e->arg(0);
  size_t size = e->arg(1);
  address_t ret_val = e->arg(2);
  CALL_ANALYSIS_FUNC2(MallocFunc, AfterRealloc, self,curr_thd_clk, inst, 
  	ptr, size, ret_val);
}

void Loader::HandleBeforeFree(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t ptr = e->arg(0);
  CALL_ANALYSIS_FUNC2(MallocFunc, BeforeFree, self,curr_thd_clk, inst, ptr);
}

void Loader::HandleAfterFree(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  address_t ptr = e->arg(0);
  CALL_ANALYSIS_FUNC2(MallocFunc, AfterFree, self,curr_thd_clk, inst, ptr);
}

void Loader::HandleBeforeValloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t size = e->arg(0);
  CALL_ANALYSIS_FUNC2(MallocFunc, BeforeValloc, self,curr_thd_clk, inst, size);
}

void Loader::HandleAfterValloc(LogEntry *e) {
  thread_t self = e->thd_id();
  timestamp_t curr_thd_clk = e->thd_clk();
  Inst *inst = sinfo_->FindInst(e->inst_id());
  DEBUG_ASSERT(inst);
  size_t size = e->arg(0);
  address_t ret_val = e->arg(1);
  CALL_ANALYSIS_FUNC2(MallocFunc, AfterValloc, self,curr_thd_clk, inst, 
  	size, ret_val);
}

} // namespace tracer