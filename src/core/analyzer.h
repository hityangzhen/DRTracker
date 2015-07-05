#ifndef __CORE_ANALYZER_H
#define __CORE_ANALYZER_H

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/descriptor.h"
#include "core/knob.h"

//Forward declarations
class CallStackInfo;

//An analyzer is used to monitor program's behaviors,but it has no
//control over the execution of the program.
class Analyzer {
public:
	Analyzer():callStackInfo(NULL),write_inst_count_(0),read_inst_count_(0),
  	lock_count_(0),volatile_count_(0),barrier_count_(0),condvar_count_(0),
  	semaphore_count_(0) { knob_=Knob::Get(); }

	virtual ~Analyzer() {}

	virtual void Register() {}
	virtual bool Enabled() {return false;}
	virtual void ProgramStart() {}
	virtual void ProgramExit() {}
	
	virtual void ImageLoad(Image *image,address_t lowAddr,
		address_t highAddr,address_t data_start,size_t dataSize,
		address_t bssStart,size_t bssSize) {}
	virtual void ImageUnload(Image *image,address_t lowAddr,
		address_t highAddr,address_t dataStart,size_t dataSize,
		address_t bssStart,size_t bssSize) {}

	virtual void ThreadStart(thread_t currThdId,thread_t parentThdId) {}
	virtual void ThreadExit(thread_t currThdId,timestamp_t currThdClk) {}
	virtual void Main(thread_t currThdId,timestamp_t currThdClk) {}
	virtual void ThreadMain(thread_t currThdId,timestamp_t currThdClk) {}

	virtual void BeforeMemRead(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) { read_inst_count_++; }
	virtual void AfterMemRead(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {}
	virtual void BeforeMemWrite(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) { write_inst_count_++; }
	virtual void AfterMemWrite(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {}

	virtual void BeforeAtomicInst(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,std::string type,
		address_t addr) { volatile_count_++; }
	virtual void AfterAtomicInst(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,std::string type,
		address_t addr) {}

	virtual void BeforeCall(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {}
	virtual void AfterCall(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target,address_t ret) {}
	virtual void BeforeReturn(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {}
	virtual void AfterReturn(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {}

	virtual void BeforePthreadCreate(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst) {}
	virtual void AfterPthreadCreate(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {}
	virtual void BeforePthreadJoin(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {}
	virtual void AfterPthreadJoin(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {}
	//execlusive lock
	virtual void BeforePthreadMutexTryLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadMutexTryLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) { lock_count_++; }
	virtual void BeforePthreadMutexLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadMutexLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) { lock_count_++; }
	virtual void BeforePthreadMutexUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) { lock_count_++; }
	virtual void AfterPthreadMutexUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	//read-write lock
	virtual void BeforePthreadRwlockTryRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadRwlockTryRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) { lock_count_++; }
	virtual void BeforePthreadRwlockRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadRwlockRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) { lock_count_++; }
	virtual void BeforePthreadRwlockTryWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadRwlockTryWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) { lock_count_++; }
	virtual void BeforePthreadRwlockWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	virtual void AfterPthreadRwlockWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) { lock_count_++; }
	virtual void BeforePthreadRwlockUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) { lock_count_++; }
	virtual void AfterPthreadRwlockUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {}
	//malloc-free
	virtual void BeforeMalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t size) {}
	virtual void AfterMalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t size,address_t addr) {}
	virtual void BeforeCalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t nmemb,size_t size) {}
	virtual void AfterCalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t nmemb,size_t size,address_t addr) {}
	virtual void BeforeRealloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t oriAddr,size_t size) {}
	virtual void AfterRealloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t oriAddr,size_t size,address_t newAddr) {}
	virtual void BeforeFree(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr) {}
	virtual void AfterFree(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr) {}
	//condition-variable
	virtual void BeforePthreadCondSignal(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) {}
  	virtual void AfterPthreadCondSignal(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) { condvar_count_++; }
  	virtual void BeforePthreadCondBroadcast(thread_t curr_thd_id,
  		timestamp_t curr_thd_clk, Inst *inst,address_t addr) {}
  	virtual void AfterPthreadCondBroadcast(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) { condvar_count_++; }
 	virtual void BeforePthreadCondWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
        address_t mutex_addr) { condvar_count_++; }
  	virtual void AfterPthreadCondWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
        address_t mutex_addr) { condvar_count_++; }
  	virtual void BeforePthreadCondTimedwait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
        address_t mutex_addr) { condvar_count_++; }
  	virtual void AfterPthreadCondTimedwait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
        address_t mutex_addr) { condvar_count_++; }
  	//barrier
  	virtual void BeforePthreadBarrierInit(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr, unsigned int count) {}
  	virtual void AfterPthreadBarrierInit(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr, unsigned int count) {}
  	virtual void BeforePthreadBarrierWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr) { barrier_count_++; }
  	virtual void AfterPthreadBarrierWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr) { barrier_count_++; }
  	//semaphore
  	virtual void BeforeSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr,unsigned int value) {}
  	virtual void AfterSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr,unsigned int value) {}
  	virtual void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {}
  	virtual void AfterSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) { semaphore_count_++; }
  	virtual void BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {}
  	virtual void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) { semaphore_count_++; }

  	virtual void SaveStatistics(const char *file_name) {}
	
	Descriptor *desc() { return &desc_; }
	void setCallStackInfo (CallStackInfo *info) { callStackInfo=info; }

protected:
	Descriptor desc_;
	Knob *knob_;
	CallStackInfo *callStackInfo;
	//for statistic
	uint64 write_inst_count_;
  	uint64 read_inst_count_;

  	uint64 lock_count_;
  	uint64 volatile_count_;
  	uint64 barrier_count_;
  	uint64 condvar_count_;
  	uint64 semaphore_count_;

	void WriteInstCountIncrease() { write_inst_count_++; }
	uint64 GetWriteInsts() { return write_inst_count_; }
	void ReadInstCountIncrease() { read_inst_count_++; }
	uint64 GetReadInsts() { return read_inst_count_; }
	void LockCountIncrease() { lock_count_++; }
	uint64 GetLocks() { return lock_count_++; }
	void BarrierCountIncrease() { barrier_count_++; }
	uint64 GetBarriers() { return barrier_count_; } 
	void CondVarCountIncrease() { condvar_count_++; }
	uint64 GetCondVars() { return condvar_count_; }
	void SemaphoreCountIncrease() { semaphore_count_++; }
	uint64 GetSemaphores() { return semaphore_count_; } 
	void VolatileCountIncrease() { volatile_count_++; }
	uint64 GetVolatiles() { return volatile_count_; } 

private:
	DISALLOW_COPY_CONSTRUCTORS(Analyzer);
};

#endif /* __CORE_ANALYZER_H */