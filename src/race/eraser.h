#ifndef __RACE_ERASER_H
#define __RACE_ERASER_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <map>
#include <tr1/unordered_map>

//Eraser algorithm
namespace race {

class Eraser:public Detector {
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	typedef std::tr1::unordered_map<address_t,uint32> LockReferenceMap;
	Eraser();
	~Eraser();

	void Register();
	bool Enabled();
	void Setup(Mutex *lock,RaceDB *race_db);

	//override
	void AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,address_t addr);
	void BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,address_t addr);
	void AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,address_t addr);
	void AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,address_t addr);
	void BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,address_t addr);
	void AfterPthreadMutexTryLock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
								Inst *inst,address_t addr,int ret_val);
	void AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
								Inst *inst,address_t addr,int ret_val);
    void AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    							Inst *inst,address_t addr,int ret_val);

    // we should discard all vc operations
    void ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id) {
    	ScopedLock lock(internal_lock_);
		atomic_map_[curr_thd_id]=false;
	}
    void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,thread_t child_thd_id) {}
	void AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
		Inst *inst,thread_t childThdId) {}

    //condition-variable
	void BeforePthreadCondSignal(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t addr) {}
  	void BeforePthreadCondBroadcast(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst, address_t addr) {}
  	void BeforePthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t cond_addr,address_t mutex_addr) {}
   	void AfterPthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
   		Inst *inst,address_t cond_addr,address_t mutex_addr) {}
  	void BeforePthreadCondTimedwait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t cond_addr,address_t mutex_addr) {}
  	void AfterPthreadCondTimedwait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t cond_addr,address_t mutex_addr) {}
  	 //barrier
  	void BeforePthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
  		Inst *inst,address_t addr) {}
  	void AfterPthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
  		Inst *inst,address_t addr) {}

  	//semaphore
  	void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk, Inst *inst,
  		address_t addr) {}
  	void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,Inst *inst,
  		address_t addr) {}
protected:
	//the meta for memory access
	class EraserMeta:public Meta{
	public:
		typedef std::map<thread_t,Inst *> InstMap;
		typedef std::set<Inst *> InstSet;
		typedef enum {
			MEM_STATE_UNKNOWN=0,
			MEM_STATE_VIRGIN,
			MEM_STATE_EXECLUSIVE,
			MEM_STATE_READ_SHARED,
			MEM_STATE_SHARED_MODIFIED
		} MemoryState;
		explicit EraserMeta(address_t addr):Meta(addr),racy(false),
			state(MEM_STATE_UNKNOWN) {}
		~EraserMeta() {}

		bool racy;
		MemoryState state;
		LockSet reader_lock_set;
		InstMap reader_inst_table;
		LockSet writer_lock_set;
		InstMap writer_inst_table;
		InstSet race_inst_set;
		thread_t thread_id;
	};

	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);
	//whether to track the racy inst
	bool track_racy_inst_;
	LockSetTable writer_lock_set_table_;
	LockSetTable reader_lock_set_table_;

	LockReferenceMap lock_reference_map_;

private:
	void UpdateMemoryState(thread_t curr_thd_id,RaceEventType race_event_type,
		EraserMeta *eraser_meta);
	LockSet *GetWriterLockSet(thread_t curr_thd_id);
	LockSet *GetReaderLockSet(thread_t curr_thd_id);

private:
	DISALLOW_COPY_CONSTRUCTORS(Eraser);
};

} //namespace race

#endif