#ifndef __CORE_DEBUG_ANALYZER_H
#define __CORE_DEBUG_ANALYZER_H

/**
 * The analyzer for debug purpose
 */

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/log.h"
#include "core/knob.h"

//Debug analyzer,print every monitored event
class DebugAnalyzer:public Analyzer {
public:
	DebugAnalyzer() {}
	~DebugAnalyzer() {}

	void Register();
	bool Enabled();
	void Setup();

	void ProgramStart() {
		INFO_FMT_PRINT_SAFE("Program Start\n");
	}

	void ProgramExit() {
		INFO_FMT_PRINT_SAFE("Program Exit\n");
	}

	void ImageLoad(Image *image,address_t lowAddr,
		address_t highAddr,address_t dataStart,size_t dataSize,
		address_t bssStart,size_t bssSize) {
		INFO_FMT_PRINT_SAFE("Image Load,name='%s',low=0x%lx,high=0x%lx,"
			"dataStart=0x%lx,dataSize=%lu,bssStart=0x%lx,bssSize=%lu\n",
			image->name().c_str(),lowAddr,highAddr,dataStart,dataSize,
			bssStart,bssSize);
	}

	void ImageUnload(Image *image, address_t lowAddr, address_t highAddr,
        address_t dataStart, size_t dataSize, address_t bssStart,
        size_t bssSize) {
    	INFO_FMT_PRINT_SAFE("Image Unload, name='%s', low=0x%lx, high=0x%lx, "
            "data_start=0x%lx, data_size=%lu, bss_start=0x%lx, bss_size=%lu\n",
            image->name().c_str(), lowAddr, highAddr,
            dataStart, dataSize, bssStart, bssSize);
  	}

  	void ThreadStart(thread_t currThdId,thread_t parentThdId) {
  		INFO_FMT_PRINT_SAFE("[T%lx] Thread Start,parent=%lx\n",
  			currThdId,parentThdId);
  	}

  	void ThreadExit(thread_t currThdId,timestamp_t currThdClk) {
  		INFO_FMT_PRINT_SAFE("[T%lx] Thread Exit\n",currThdId);	
  	}

	void Main(thread_t currThdId,timestamp_t currThdClk) {
		INFO_FMT_PRINT_SAFE("[T%lx] Main Func\n",currThdId);
	}

	void ThreadMain(thread_t currThdId,timestamp_t currThdClk) {
		INFO_FMT_PRINT_SAFE("[T%lx] Thread Main Func\n",currThdId);
	}

	void BeforeMemRead(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {
		INFO_FMT_PRINT_SAFE(
			"[T%lx] Before Read,inst='%s',addr=0x%lx,size=%lu,clk=%lx\n",
			currThdId,inst->ToString().c_str(),addr,size,currThdClk);
	}

	void AfterMemRead(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {
		INFO_FMT_PRINT_SAFE(
			"[T%lx] After Read,inst='%s',addr=0x%lx,size=%lu\n",
			currThdId,inst->ToString().c_str(),addr,size);
	}

	void BeforeMemWrite(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {
		INFO_FMT_PRINT_SAFE(
			"[T%lx] Before Write,inst='%s',addr=0x%lx,size=%lu,clk=%lx\n",
			currThdId,inst->ToString().c_str(),addr,size,currThdClk);
	}

	void AfterMemWrite(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr,size_t size) {
		INFO_FMT_PRINT_SAFE(
			"[T%lx] After Write,inst='%s',addr=0x%lx,size=%lu\n",
			currThdId,inst->ToString().c_str(),addr,size);	
	}

	void BeforeAtomicInst(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,std::string type,
		address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before Atomic Inst, inst='%s', type='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), type.c_str(), addr);
	}

	void AfterAtomicInst(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,std::string type,
		address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Atomic Inst, inst='%s', type='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), type.c_str(), addr);	
	}

	void BeforeCall(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before Call, inst='%s', target=0x%lx\n",
        	currThdId, inst->ToString().c_str(), target);
	}

	void AfterCall(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target,address_t ret) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Call, inst='%s', target=0x%lx, ret=0x%lx\n",
        	currThdId, inst->ToString().c_str(), target, ret);
	}

	void BeforeReturn(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before Return, inst='%s', target=0x%lx\n",
        	currThdId, inst->ToString().c_str(), target);
	}

	void AfterReturn(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t target) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Return, inst='%s', target=0x%lx\n",
        	currThdId, inst->ToString().c_str(), target);
	}

	void BeforePthreadCreate(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst) {
		INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadCreate, inst='%s'\n",
            currThdId, inst->ToString().c_str());
	}

	void AfterPthreadCreate(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {
		INFO_FMT_PRINT_SAFE("[T%lx] After PthreadCreate, inst='%s', child=%lx\n",
        	currThdId, inst->ToString().c_str(), childThdId);
	}

	void BeforePthreadJoin(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {
		INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadJoin, inst='%s', child=%lx\n",
        	currThdId, inst->ToString().c_str(), childThdId);
	}

	void AfterPthreadJoin(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,thread_t childThdId) {
		INFO_FMT_PRINT_SAFE("[T%lx] After PthreadJoin, inst='%s', child=%lx\n",
            currThdId, inst->ToString().c_str(), childThdId);
	}

	void BeforePthreadMutexTryLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadMutexTryLock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}

	void AfterPthreadMutexTryLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadMutexTryLock, inst='%s', addr=0x%lx, ret_val=%d\n",
        	currThdId, inst->ToString().c_str(), addr, retVal);
	}

	void BeforePthreadMutexLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadMutexLock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}

	void AfterPthreadMutexLock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadMutexLock, inst='%s', addr=0x%lx, clk=%lx\n",
        	currThdId, inst->ToString().c_str(), addr,currThdClk);
	}

	void BeforePthreadMutexUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadMutexUnlock, inst='%s', addr=0x%lx, clk=%lx\n",
        	currThdId, inst->ToString().c_str(), addr, currThdClk);
	}

	void AfterPthreadMutexUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadMutexUnlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}
	//read-write lock
	void BeforePthreadRwlockTryRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadRwlockTryRdlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}

	void AfterPthreadRwlockTryRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadRwlockTryRdlock, inst='%s', addr=0x%lx, ret_val=%d\n",
        	currThdId, inst->ToString().c_str(), addr, retVal);
	}
	void BeforePthreadRwlockRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadRwlockRdlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}
	void AfterPthreadRwlockRdlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadRwlockRdlock, inst='%s', addr=0x%lx, clk=%lx\n",
        	currThdId, inst->ToString().c_str(), addr,currThdClk);
	}
	void BeforePthreadRwlockTryWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadRwlockTryWrlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}
	void AfterPthreadRwlockTryWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr,int retVal) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadRwlockTryWrlock, inst='%s', addr=0x%lx, ret_val=%d\n",
        	currThdId, inst->ToString().c_str(), addr, retVal);
	}
	void BeforePthreadRwlockWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadRwlockWrlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}
	void AfterPthreadRwlockWrlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadRwlockWrlock, inst='%s', addr=0x%lx, clk=%lx\n",
        	currThdId, inst->ToString().c_str(), addr,currThdClk);
	}
	void BeforePthreadRwlockUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before PthreadRwlockUnlock, inst='%s', addr=0x%lx, clk=%lx\n",
        	currThdId, inst->ToString().c_str(), addr,currThdClk);
	}
	void AfterPthreadRwlockUnlock(thread_t currThdId,
		timestamp_t currThdClk,Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After PthreadRwlockUnlock, inst='%s', addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), addr);
	}
	//malloc
	void BeforeMalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t size) {
		INFO_FMT_PRINT_SAFE(
      		"[T%lx] Before Malloc, inst='%s', size=%lu\n",
      		currThdId, inst->ToString().c_str(), size);
	}

	void AfterMalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t size,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Malloc, inst='%s', size=%lu, addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), size, addr);
	}

	void BeforeCalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t nmemb,size_t size) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before Calloc, inst='%s', nmemb=%lu, size=%lu\n",
        	currThdId, inst->ToString().c_str(), nmemb, size);
	}

	void AfterCalloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,size_t nmemb,size_t size,address_t addr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Calloc, inst='%s', nmemb=%lu, size=%lu, addr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), nmemb, size, addr);
	}

	void BeforeRealloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t oriAddr,size_t size) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] Before Realloc, inst='%s', oriAddr=0x%lx, size=%lu\n",
        	currThdId, inst->ToString().c_str(), oriAddr, size);
	}

	void AfterRealloc(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t oriAddr,size_t size,address_t newAddr) {
		INFO_FMT_PRINT_SAFE(
        	"[T%lx] After Realloc, inst='%s', oriAddr=0x%lx, "
        	"size=%lu, newAddr=0x%lx\n",
        	currThdId, inst->ToString().c_str(), oriAddr, size, newAddr);
	}

	void BeforeFree(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE("[T%lx] Before Free, inst='%s', addr=0x%lx\n",
            currThdId, inst->ToString().c_str(), addr);
	}

	void AfterFree(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,address_t addr) {
		INFO_FMT_PRINT_SAFE("[T%lx] After Free, inst='%s', addr=0x%lx\n",
            currThdId, inst->ToString().c_str(), addr);
	}

	void BeforePthreadCondSignal(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadCondSignal, inst='%s'," 
    	"addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}

  	void AfterPthreadCondSignal(thread_t curr_thd_id, 
  		timestamp_t curr_thd_clk,Inst *inst, address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] After PthreadCondSignal, inst='%s',"
    	" addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}

  	void BeforePthreadCondBroadcast(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadCondBroadcast, "
    	"inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  	}

  	void AfterPthreadCondBroadcast(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] After PthreadCondBroadcast, "
    	"inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  	}

  	void BeforePthreadCondWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t cond_addr, address_t mutex_addr) {
    	INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondWait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  	}

  	void AfterPthreadCondWait(thread_t curr_thd_id, timestamp_t curr_thd_clk,
        Inst *inst, address_t cond_addr,address_t mutex_addr) {
    	INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondWait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  	}

  	void BeforePthreadCondTimedwait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t cond_addr, address_t mutex_addr) {
    	INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondTimedwait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  	}

  	void AfterPthreadCondTimedwait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t cond_addr, address_t mutex_addr) {
    	INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondTimedwait, inst='%s', "
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  	}

  	void BeforePthreadBarrierInit(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr, unsigned int count) {
	    INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadBarrierInit, inst='%s',"
	    " addr=0x%lx, count=%u\n",curr_thd_id, inst->ToString().c_str(), 
	    addr, count);
	}

  	void AfterPthreadBarrierInit(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,
        address_t addr, unsigned int count) {
	    INFO_FMT_PRINT_SAFE("[T%lx] After PthreadBarrierInit, inst='%s',"
	    " addr=0x%lx\n, count=%u\n",curr_thd_id, inst->ToString().c_str(),
	    addr, count);
	}

  	void BeforePthreadBarrierWait(thread_t curr_thd_id,
       	timestamp_t curr_thd_clk, Inst *inst,address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadBarrierWait, "
    	"inst='%s', addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}

  	void AfterPthreadBarrierWait(thread_t curr_thd_id,
        timestamp_t curr_thd_clk, Inst *inst,address_t addr) {
    	INFO_FMT_PRINT_SAFE("[T%lx] After PthreadBarrierWait, inst='%s',"
    	" addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}
  	//semaphore
  	void BeforeSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr,unsigned int value) {
  		INFO_FMT_PRINT_SAFE("[T%lx] Before SemInit, inst='%s',"
	    " addr=0x%lx, value=%u\n",curr_thd_id, inst->ToString().c_str(), 
	    addr, value);
  	}
  	void AfterSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr,unsigned int value) {
  		INFO_FMT_PRINT_SAFE("[T%lx] After SemInit, inst='%s',"
	    " addr=0x%lx, value=%u\n",curr_thd_id, inst->ToString().c_str(), 
	    addr, value);	
  	}
  	void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {
  		INFO_FMT_PRINT_SAFE("[T%lx] Before SemPost, inst='%s',"
	    " addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}
  	void AfterSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {
  		INFO_FMT_PRINT_SAFE("[T%lx] After SemPost, inst='%s',"
	    " addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}
  	void BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {
  		INFO_FMT_PRINT_SAFE("[T%lx] Before SemWait, inst='%s',"
	    " addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);
  	}
  	void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr) {
  		INFO_FMT_PRINT_SAFE("[T%lx] After SemWait, inst='%s',"
	    " addr=0x%lx\n",curr_thd_id, inst->ToString().c_str(), addr);	
  	}
private:
	DISALLOW_COPY_CONSTRUCTORS(DebugAnalyzer);
};

#endif /* __CORE_DEBUG_ANALYZER_H */