#ifndef _RACE_DETECTOR_H
#define _RACE_DETECTOR_H

//Define the abstract data race detector

#include <tr1/unordered_map>
#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/vector_clock.h"
#include "core/filter.h"
#include "race/race.h"
#include "core/lock_set.h"
#include "race/adhoc_sync.h"
#include "race/loop.h"
#include "race/cond_wait.h"
#include "core/event.h"
#include "core/log.h"

#define EVENT_HANDLE_ARG_0
#define EVENT_HANDLE_ARG_1 event->arg0_
#define EVENT_HANDLE_ARG_2 EVENT_HANDLE_ARG_1, event->arg1_
#define EVENT_HANDLE_ARG_3 EVENT_HANDLE_ARG_2, event->arg2_
#define EVENT_HANDLE_ARG_4 EVENT_HANDLE_ARG_3, event->arg3_
#define EVENT_HANDLE_ARG_5 EVENT_HANDLE_ARG_4, event->arg4_
#define EVENT_HANDLE_ARG_6 EVENT_HANDLE_ARG_5, event->arg5_
#define EVENT_HANDLE_ARG_7 EVENT_HANDLE_ARG_6, event->arg6_
#define EVENT_HANDLE_ARG(i) EVENT_HANDLE_ARG_##i 

#define EVENT_HANDLE(Name,NUM_ARGS)                                     \
  static void Name##EventHandle (Detector *dtc,EventBase *eb)  {        \
    EVENT_CLASS(Name) *event = (EVENT_CLASS(Name) *) eb;                \
    dtc->Name(EVENT_HANDLE_ARG(NUM_ARGS));                              \
    DELETE_EVENT(event);                                                \
  }

//class static function pointer
#define REGISTER_EVENT_HANDLE(Name)                                     \
  event_handle_table_[#Name]=&Detector::Name##EventHandle

namespace race {

class Detector:public Analyzer {
public:

	Detector();
	virtual ~Detector();

	virtual void Register();
	virtual bool Enabled()=0;
	virtual void Setup(Mutex *lock,RaceDB *race_db);

	virtual void ImageLoad(Image *image,address_t low_addr, address_t high_addr,
    address_t data_start, size_t data_size,address_t bss_start, size_t bss_size);
  EVENT_HANDLE(ImageLoad,7);
  virtual void ImageUnload(Image *image,address_t low_addr, address_t high_addr,
    address_t data_start, size_t data_size,address_t bss_start, size_t bss_size);
  EVENT_HANDLE(ImageUnload,7);
  //thread start-exit
  virtual void ThreadStart(thread_t curr_thd_id, thread_t parent_thd_id);
  EVENT_HANDLE(ThreadStart,2);
  virtual void ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk);
  EVENT_HANDLE(ThreadExit,2);
  //read-write
  virtual void BeforeMemRead(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t addr, size_t size);
  EVENT_HANDLE(BeforeMemRead,5);
  virtual void BeforeMemWrite(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t addr, size_t size);
  EVENT_HANDLE(BeforeMemWrite,5);
  
  //atomic inst
  virtual void BeforeAtomicInst(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,std::string type, address_t addr);
  EVENT_HANDLE(BeforeAtomicInst,5);
  virtual void AfterAtomicInst(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,std::string type, address_t addr);
  EVENT_HANDLE(AfterAtomicInst,5);

  //thread create-join
  virtual void AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
  	Inst *inst,thread_t childThdId);
  EVENT_HANDLE(AfterPthreadCreate,4);
  virtual void BeforePthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,thread_t child_thd_id);
  EVENT_HANDLE(BeforePthreadJoin,4);
  virtual void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,thread_t child_thd_id);
  EVENT_HANDLE(AfterPthreadJoin,4);
  //call-return
  virtual void BeforeCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target);
  EVENT_HANDLE(BeforeCall,4);
  virtual void AfterCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target,address_t ret);
  EVENT_HANDLE(AfterCall,5);
  virtual void BeforeReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target);
  EVENT_HANDLE(BeforeReturn,4);
  virtual void AfterReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target);
  EVENT_HANDLE(AfterReturn,4);

  //mutex lock
  virtual void BeforePthreadMutexTryLock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadMutexTryLock,4);
  virtual void BeforePthreadMutexLock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadMutexLock,4);
  virtual void AfterPthreadMutexLock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst, address_t addr);
  EVENT_HANDLE(AfterPthreadMutexLock,4);
  virtual void BeforePthreadMutexUnlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadMutexUnlock,4);
  virtual void AfterPthreadMutexTryLock(thread_t currThdId,
    timestamp_t currThdClk,Inst *inst,address_t addr,int ret_val);
  EVENT_HANDLE(AfterPthreadMutexTryLock,5);
  //read-write lock
  virtual void BeforePthreadRwlockRdlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadRwlockRdlock,4);
	virtual void AfterPthreadRwlockRdlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr) ;
  EVENT_HANDLE(AfterPthreadRwlockRdlock,4);
  virtual void BeforePthreadRwlockWrlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadRwlockWrlock,4);
	virtual void AfterPthreadRwlockWrlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr) ;
  EVENT_HANDLE(AfterPthreadRwlockWrlock,4);
	virtual void BeforePthreadRwlockUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr) ;
  EVENT_HANDLE(BeforePthreadRwlockUnlock,4);
  virtual void BeforePthreadRwlockTryRdlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadRwlockTryRdlock,4);
  virtual void AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val);
  EVENT_HANDLE(AfterPthreadRwlockTryRdlock,5);
  virtual void BeforePthreadRwlockTryWrlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadRwlockTryWrlock,4);
  virtual void AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val);
  EVENT_HANDLE(AfterPthreadRwlockTryWrlock,5);

	//condition-variable
	virtual void BeforePthreadCondSignal(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadCondSignal,4);
  virtual void BeforePthreadCondBroadcast(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst, address_t addr);
  EVENT_HANDLE(BeforePthreadCondBroadcast,4);
  virtual void BeforePthreadCondWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr, 
    address_t mutex_addr);
  EVENT_HANDLE(BeforePthreadCondWait,5);
  virtual void AfterPthreadCondWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr, 
    address_t mutex_addr);
  EVENT_HANDLE(AfterPthreadCondWait,5);
  virtual void BeforePthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr);
  EVENT_HANDLE(BeforePthreadCondTimedwait,5);
  virtual void AfterPthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr);
  EVENT_HANDLE(AfterPthreadCondTimedwait,5);
  //malloc-free
  virtual void AfterMalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, size_t size, address_t addr);
  EVENT_HANDLE(AfterMalloc,5);
  virtual void AfterCalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, size_t nmemb, size_t size,address_t addr);
  EVENT_HANDLE(AfterCalloc,6);
  virtual void BeforeRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t ori_addr, size_t size);
  EVENT_HANDLE(BeforeRealloc,5);
  virtual void AfterRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t ori_addr, size_t size,address_t new_addr);
  EVENT_HANDLE(AfterRealloc,6);
  virtual void BeforeFree(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t addr);
  EVENT_HANDLE(BeforeFree,4);
  //barrier
  virtual void BeforePthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  EVENT_HANDLE(BeforePthreadBarrierWait,4);
  virtual void AfterPthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  EVENT_HANDLE(AfterPthreadBarrierWait,4);
  //semaphore
  virtual void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr);
  EVENT_HANDLE(BeforeSemPost,4);
  virtual void BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr);
  EVENT_HANDLE(BeforeSemWait,4);
  virtual void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr);
  EVENT_HANDLE(AfterSemWait,4);

  address_t GetUnitSize() {
    return unit_size_;
  }
  void SaveStatistics(const char *file_name);
  //unified event handle function pointer
  typedef void (*EventHandle)(Detector *,EventBase *);
  static EventHandle GetEventHandle(std::string name) {
    if(event_handle_table_.find(name)==event_handle_table_.end())
        return NULL;
    return event_handle_table_[name];
  }

  static std::map<std::string,EventHandle> event_handle_table_;
protected:
  typedef std::map<int,Loop> LoopTable;
  typedef std::tr1::unordered_map<std::string,LoopTable *> LoopMap;
	//the abstract meta data for the memory
	class Meta {
	public:
		typedef std::tr1::unordered_map<address_t,Meta *> Table;
		explicit Meta(address_t a):addr(a) {}
		virtual ~Meta() {}

		address_t addr;
	};

	//the meta data for mutex variables to track vector clock
	class MutexMeta {
	public:
		typedef std::tr1::unordered_map<address_t,MutexMeta *> Table;
		MutexMeta() {}
		virtual ~MutexMeta() {}

		VectorClock vc;
	};
  
	//the meta data for condition variables

  class CondMeta {
  public:
    typedef std::map<thread_t,VectorClock> VectorClockMap;
    typedef std::tr1::unordered_map<address_t,CondMeta *> Table;

    CondMeta() {}
    ~CondMeta() {}

    VectorClockMap wait_table;
    VectorClockMap signal_table;
  };

	//the meta data for barrier variables
  class BarrierMeta {
  public:
    typedef std::tr1::unordered_map<address_t,BarrierMeta *> Table;

    BarrierMeta():waiter(0) {}
    ~BarrierMeta() {}

    VectorClock wait_vc;
    int waiter;
  };

  //semaphore
  class SemMeta {
  public:
    typedef std::tr1::unordered_map<address_t,SemMeta *> Table;
    SemMeta() {}
    virtual ~SemMeta() {}

    VectorClock vc;
  };

	void AllocAddrRegion(address_t addr,size_t size);
	void FreeAddrRegion(address_t addr);
	bool FilterAccess(address_t addr) {return filter_->Filter(addr,false);}

	void ReportRace(Meta *meta,thread_t t0,Inst *i0,RaceEventType p0,
		thread_t t1,Inst *i1,RaceEventType p1);

  void PrintDebugRaceInfo(const char *detector_name,RaceType race_type,
  Meta *meta,thread_t curr_thd_id,Inst *curr_inst);

	//main processing function
  virtual MutexMeta *GetMutexMeta(address_t addr);
  virtual Meta *GetMeta(address_t iaddr)=0;
  virtual CondMeta *GetCondMeta(address_t iaddr);
  virtual BarrierMeta *GetBarrierMeta(address_t iaddr);
  virtual SemMeta *GetSemMeta(address_t iaddr);

	virtual void ProcessLock(thread_t curr_thd_id,MutexMeta *meta);
	virtual void ProcessUnlock(thread_t curr_thd_id,MutexMeta *meta);

	virtual void ProcessNotify(thread_t curr_thd_id,CondMeta *meta);
  virtual void ProcessPreWait(thread_t curr_thd_id,CondMeta *meta);
  virtual void ProcessPostWait(thread_t curr_thd_id,CondMeta *meta);

  virtual void ProcessPreBarrier(thread_t curr_thd_id,BarrierMeta *meta);
  virtual void ProcessPostBarrier(thread_t curr_thd_id,BarrierMeta *meta);

  virtual void ProcessBeforeSemPost(thread_t curr_thd_id,SemMeta *meta);
  virtual void ProcessAfterSemWait(thread_t curr_thd_id,SemMeta *meta);

	virtual void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)=0;
	virtual void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)=0;

	virtual void ProcessFree(Meta *meta)=0;
  virtual void ProcessFree(MutexMeta *meta);
  virtual void ProcessFree(CondMeta *meta);
  virtual void ProcessFree(BarrierMeta *meta);
  virtual void ProcessFree(SemMeta *meta);

  //dynamic ah-hoc read identify
  AdhocSync::WriteMeta *ProcessAdhocRead(thread_t curr_thd_id,Inst *rd_inst,
    address_t start_addr,address_t end_addr,
    std::set<AdhocSync::ReadMeta *> &result);

  //spinning read wrapper functions
  void ProcessSRLSync(thread_t curr_thd_id,Inst *curr_inst);
  void ProcessSRLRead(thread_t curr_thd_id,Inst *curr_inst,address_t addr);
  void ProcessSRLWrite(thread_t curr_thd_id,Inst *curr_inst,address_t addr);
  //cond_wait wrapper functions
  void ProcessLockSignalWrite(thread_t curr_thd_id,address_t addr);
  bool ProcessCWLRead(thread_t curr_thd_id,Inst *curr_inst,address_t addr);
  bool ProcessCWLRead(thread_t curr_thd_id,Inst *curr_inst,address_t addr,
    std::string &file_name,int line);
  void ProcessCWLCalledFuncWrite(thread_t curr_thd_id,address_t addr);
  void ProcessCWLSync(thread_t curr_thd_id,Inst *curr_inst);

	Mutex *internal_lock_;
	RaceDB *race_db_;
	address_t unit_size_;
	RegionFilter *filter_;
	//
	MutexMeta::Table mutex_meta_table_;
	Meta::Table meta_table_;
  CondMeta::Table cond_meta_table_;
  BarrierMeta::Table barrier_meta_table_;
  SemMeta::Table sem_meta_table_;

	std::map<thread_t,VectorClock *> curr_vc_map_;
	std::map<thread_t,bool> atomic_map_; //whether executing atomic inst.
  uint64 vc_mem_size_;
  //dynamic ad-hoc
  AdhocSync *adhoc_sync_;
  //loop info
  LoopDB *loop_db_;
  //cond_wait info
  CondWaitDB *cond_wait_db_;
};


} //namespace race

#endif