#ifndef __CORE_EXECUTION_CONTROL_HPP
#define __CORE_EXECUTION_CONTROL_HPP

#include <list>
#include <map>

#include "pin.H"

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/static_info.h"
#include "core/descriptor.h"
#include "core/analyzer.h"
#include "core/debug_analyzer.h"
#include "core/callstack.h"
#include "core/pin_sync.hpp"
#include "core/pin_knob.h"
#include "core/wrapper.hpp"

//Define macros for calling analysis functions.
#define CALL_ANALYSIS_FUNC(func,...)											\
	for(AnalyzerContainer::iterator it=analyzers_.begin();						\
		it!=analyzers_.end();it++) {											\
		(*it)->func(__VA_ARGS__);												\
	}

#define CALL_ANALYSIS_FUNC2(type,func,...)										\
	for(AnalyzerContainer::iterator it=analyzers_.begin();						\
		it!=analyzers_.end();it++) {											\
		if((*it)->desc()->Hook##type())											\
			(*it)->func(__VA_ARGS__);											\
	}

//Define macros for wrapper handlers.
#define MEMBER_WRAPPER_HANDLER(name) Handle##name
#define STATIC_WRAPPER_HANDLER(name) __##name

//Wrapper handlers function declaration
#define DECLARE_MEMBER_WRAPPER_HANDLER(name)									\
	protected:																	\
		virtual void MEMBER_WRAPPER_HANDLER(name)(WRAPPER_CLASS(name) *wrapper)

#define DECLARE_STATIC_WRAPPER_HANDLER(name)									\
	protected:																	\
		static void STATIC_WRAPPER_HANDLER(name)(WRAPPER_CLASS(name) *wrapper) {\
			ctrl_->HandleBeforeWrapper(wrapper);								\
			ctrl_->MEMBER_WRAPPER_HANDLER(name)(wrapper);						\
			ctrl_->HandleAfterWrapper(wrapper);									\
		}

#define DECLARE_WRAPPER_HANDLER(name)											\
	DECLARE_MEMBER_WRAPPER_HANDLER(name);										\
	DECLARE_STATIC_WRAPPER_HANDLER(name);

#define IMPLEMENT_WRAPPER_HANDLER(name,cls)										\
	void cls::MEMBER_WRAPPER_HANDLER(name)(WRAPPER_CLASS(name) * wrapper)

#define ACTIVATE_WRAPPER_HANDLER(name)											\
	ACTIVATE_WRAPPER(name,img,STATIC_WRAPPER_HANDLER(name))

//Define macros for the main entry functions.
#define MAIN_ENTRY(controller)													\
	static controller *ctrl=new controller;										\
	static VOID I_InstrumentTrace(TRACE trace,VOID *v) {						\
		ctrl->InstrumentTrace(trace,v);											\
	}																			\
	static VOID I_ImageLoad(IMG img,VOID *v) {									\
		ctrl->ImageLoad(img,v);													\
	}																			\
	static VOID I_ImageUnload(IMG img,VOID *v) {								\
		ctrl->ImageUnload(img,v);												\
	}																			\
	static VOID I_ContextChange(THREADID tid,									\
		CONTEXT_CHANGE_REASON reason,											\
		const CONTEXT *from,													\
		CONTEXT *to,															\
		INT32 info,																\
		VOID *v) {																\
		ctrl->ContextChange(tid,reason,from,to,info,v);							\
	}																			\
	static VOID I_ProgramStart() {												\
		ctrl->ProgramStart();													\
	}																			\
	static VOID I_ProgramExit(INT32 code,VOID *v) {								\
		ctrl->ProgramExit(code,v);												\
	}																			\
	static VOID I_ThreadStart(THREADID tid,										\
		CONTEXT *ctxt,															\
		INT32 flags,															\
		VOID *v) {																\
		ctrl->ThreadStart(tid,ctxt,flags,v);									\
	}																			\
	static VOID I_ThreadExit(THREADID tid,										\
		const CONTEXT *ctxt,													\
		INT32 code,																\
		VOID *v) {																\
		ctrl->ThreadExit(tid,ctxt,code,v);										\
	}																			\
	int main(int argc,char *argv[]) {											\
		ctrl->Initialize();														\
		ctrl->PreSetup();														\
																				\
		PIN_InitSymbols();														\
		PIN_Init(argc,argv);													\
		ctrl->PostSetup();														\
		TRACE_AddInstrumentFunction(I_InstrumentTrace,NULL);					\
		IMG_AddInstrumentFunction(I_ImageLoad,NULL);							\
		IMG_AddUnloadFunction(I_ImageUnload,NULL);								\
		PIN_AddContextChangeFunction(I_ContextChange,NULL);						\
		PIN_AddFiniFunction(I_ProgramExit,NULL);								\
		PIN_AddThreadStartFunction(I_ThreadStart,NULL);							\
		PIN_AddThreadFiniFunction(I_ThreadExit,NULL);							\
		I_ProgramStart();														\
		PIN_StartProgram();														\
	}																				

//The main controller for the dynamic program analysis.
class ExecutionControl {
public:
	ExecutionControl();
	virtual ~ExecutionControl() {}

	void Initialize();
	void PreSetup();
	void PostSetup();
	void InstrumentTrace(TRACE trace,VOID *v);
	void ImageLoad(IMG img,VOID *v);
	void ImageUnload(IMG img,VOID *v);
	void ContextChange(THREADID tid,CONTEXT_CHANGE_REASON reason,
		const CONTEXT *from,CONTEXT *to,INT32 info,VOID *V);
	void ProgramStart();
	void ProgramExit(INT32 code,VOID *V);
	void ThreadStart(THREADID tid,CONTEXT *ctxt,INT32 FLAGS,VOID *v);
	void ThreadExit(THREADID tid,const CONTEXT *ctxt,INT32 code,VOID *v);

protected:
	typedef std::list<Analyzer *>AnalyzerContainer;

	virtual Mutex *CreateMutex() { return new PinMutex; }
	virtual Semaphore *CreateSemaphore(unsigned int value) {
		return new SysSemaphore(value);
	}

	//Define wrapper handle functions.
	virtual void HandlePreSetup();
	virtual void HandlePostSetup();
	virtual bool HandleIgnoreInstCount(IMG) {return true;}
	virtual bool HandleIgnoreMemAccess(IMG img) { return false; }
	virtual void HandlePreInstrumentTrace(TRACE trace);
	virtual void HandlePostInstrumentTrace(TRACE trace);
	virtual void HandleProgramStart();
	virtual void HandleProgramExit();
	virtual void HandleImageLoad(IMG img,Image *image);
	virtual void HandleImageUnload(IMG img,Image *image);
	virtual void HandleThreadStart();
	virtual void HandleThreadExit();
	virtual void HandleMain(THREADID tid,CONTEXT *ctxt);
	virtual void HandleThreadMain(THREADID tid,CONTEXT *ctxt);
	virtual void HandleBeforeMemRead(THREADID tid, Inst *inst, address_t addr,
                           			size_t size);
  	virtual void HandleAfterMemRead(THREADID tid, Inst *inst, address_t addr,
                                  	size_t size);
  	virtual void HandleBeforeMemWrite(THREADID tid, Inst *inst, address_t addr,
                                    size_t size);
  	virtual void HandleAfterMemWrite(THREADID tid, Inst *inst, address_t addr,
                                   	size_t size);
  	virtual void HandleBeforeAtomicInst(THREADID tid, Inst *inst, OPCODE opcode,
                                    address_t addr);
  	virtual void HandleAfterAtomicInst(THREADID tid, Inst *inst, OPCODE opcode,
                                    address_t addr);
  	virtual void HandleBeforeCall(THREADID tid, Inst *inst, address_t target);
  	virtual void HandleAfterCall(THREADID tid, Inst *inst, address_t target,
                               address_t ret);
  	virtual void HandleBeforeReturn(THREADID tid, Inst *inst, address_t target);
 	virtual void HandleAfterReturn(THREADID tid, Inst *inst, address_t target);

 	//before and after the wrapper function handler
 	virtual void HandleBeforeWrapper(WrapperBase *wrapper);
  	virtual void HandleAfterWrapper(WrapperBase *wrapper);


  	void LockKernel() { kernel_lock_->Lock(); }
  	void UnlockKernel() { kernel_lock_->Unlock(); }
  	void Abort(const std::string &msg);
  	Inst *GetInst(ADDRINT pc);
  	void UpdateInstOpcode(Inst *inst,INS ins);
  	void UpdateInstDebugInfo(Inst *inst,ADDRINT pc);
  	void AddAnalyzer(Analyzer *analyzer);
  	thread_t GetThdId(pthread_t thread);
  	thread_t GetParent();
  	thread_t Self() { return PIN_ThreadUid(); }
  	timestamp_t GetThdClk(THREADID tid) { return tls_thd_clock_[tid]; }

  	thread_t WaitForNewChild(WRAPPER_CLASS(PthreadCreate) *wrapper);
  	void ReplacePthreadCreateWrapper(IMG img);
  	void ReplacePthreadWrappers(IMG img);
  	void ReplaceMallocWrappers(IMG img);

  	Mutex *kernel_lock_;
 	Knob *knob_;
 	Descriptor desc_;
 	LogFile *debug_file_;
 	CallStackInfo *callstack_info_;
 	DebugAnalyzer *debug_analyzer_;
 	StaticInfo *sinfo_;
 	AnalyzerContainer analyzers_;
 	bool main_thread_started_;
 	timestamp_t tls_thd_clock_[PIN_MAX_THREADS];
 	thread_t main_thd_id_;

 	address_t tls_read_addr_[PIN_MAX_THREADS];
 	size_t tls_read_size_[PIN_MAX_THREADS];
 	address_t tls_write_addr_[PIN_MAX_THREADS];
 	size_t tls_write_size_[PIN_MAX_THREADS];
 	address_t tls_read2_addr_[PIN_MAX_THREADS];
 	size_t tls_read2_size_[PIN_MAX_THREADS];
 	address_t tls_atomic_addr_[PIN_MAX_THREADS];

 	std::map<OS_THREAD_ID,Semaphore *> thd_create_sem_map_; //init=0
 	std::map<OS_THREAD_ID,thread_t> child_thd_map_;
 	std::map<OS_THREAD_ID,thread_t> os_tid_map_;
 	std::map<pthread_t,thread_t> pthread_handle_map_;

 	static ExecutionControl *ctrl_;
 private:
 	void InstrumentStartupFunc(IMG img);

 	static void __Main(THREADID tid,CONTEXT *ctxt);
 	static void __ThreadMain(THREADID ,CONTEXT *ctxt);
 	static void __BeforeMemRead(THREADID tid, Inst *inst, ADDRINT addr,
                           			UINT32 size);
 	static void __AfterMemRead(THREADID tid, Inst *inst);
 	static void __BeforeMemWrite(THREADID tid, Inst *inst, ADDRINT addr,
                           			UINT32 size);
 	static void __AfterMemWrite(THREADID tid,Inst *inst);
 	static void __BeforeMemRead2(THREADID tid,Inst *inst,ADDRINT addr,
 									UINT32 size);
 	static void __AfterMemRead2(THREADID tid,Inst *inst);
 	static void __BeforeAtomicInst(THREADID tid, Inst *inst, UINT32 opcode,
                                 ADDRINT addr);
  	static void __AfterAtomicInst(THREADID tid, Inst *inst, UINT32 opcode);
  	static void __BeforeCall(THREADID tid, Inst *inst, ADDRINT target);
  	static void __AfterCall(THREADID tid, Inst *inst, ADDRINT target,
                          ADDRINT ret);
  	static void __BeforeReturn(THREADID tid, Inst *inst, ADDRINT target);
  	static void __AfterReturn(THREADID tid, Inst *inst, ADDRINT target);

  	DISALLOW_COPY_CONSTRUCTORS(ExecutionControl);


  	//Declare wrapper handlers.
  	DECLARE_WRAPPER_HANDLER(PthreadCreate);
  	DECLARE_WRAPPER_HANDLER(PthreadJoin);
  	DECLARE_WRAPPER_HANDLER(PthreadMutexTryLock);
  	DECLARE_WRAPPER_HANDLER(PthreadMutexLock);
  	DECLARE_WRAPPER_HANDLER(PthreadMutexUnlock);

 	DECLARE_WRAPPER_HANDLER(PthreadRwlockTryRdlock);
	DECLARE_WRAPPER_HANDLER(PthreadRwlockTryWrlock);
	DECLARE_WRAPPER_HANDLER(PthreadRwlockRdlock);
	DECLARE_WRAPPER_HANDLER(PthreadRwlockWrlock);
	DECLARE_WRAPPER_HANDLER(PthreadRwlockUnlock);

	DECLARE_WRAPPER_HANDLER(PthreadCondSignal);
  	DECLARE_WRAPPER_HANDLER(PthreadCondBroadcast);
  	DECLARE_WRAPPER_HANDLER(PthreadCondWait);
  	DECLARE_WRAPPER_HANDLER(PthreadCondTimedwait);

  	DECLARE_WRAPPER_HANDLER(PthreadBarrierWait);
  	DECLARE_WRAPPER_HANDLER(PthreadBarrierInit);

	DECLARE_WRAPPER_HANDLER(SemInit);
	DECLARE_WRAPPER_HANDLER(SemPost);
	DECLARE_WRAPPER_HANDLER(SemWait);
  	
  	DECLARE_WRAPPER_HANDLER(Malloc);
  	DECLARE_WRAPPER_HANDLER(Calloc);
  	DECLARE_WRAPPER_HANDLER(Realloc);
  	DECLARE_WRAPPER_HANDLER(Free);
};

#endif /* __CORE_EXECUTION_CONTROL_HPP */
