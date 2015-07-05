#ifndef __RACE_RACE_TRACK_H
#define __RACE_RACE_TRACK_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"

#include <map>
#include <set>

//Similary to Eraser.

namespace race {

class RaceTrack:public Detector {
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	typedef std::tr1::unordered_map<address_t,uint32> LockReferenceMap;
	RaceTrack();
	~RaceTrack();

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

protected:
	class RtMeta:public Meta {
	public:
		typedef std::map<thread_t,Inst *> InstMap;
		typedef std::set<Inst *> InstSet;
		typedef std::map<thread_t,timestamp_t> ThreadSet;
		typedef enum {
			MEM_STATE_VIRGIN=0,
			MEM_STATE_EXCLUSIVE0,
			MEM_STATE_EXCLUSIVE1,
			MEM_STATE_READ_SHARED,
			MEM_STATE_SHARED_MODIFIED1,
			MEM_STATE_EXCLUSIVE2,
			MEM_STATE_SHARED_MODIFIED2,
			MEM_STATE_RACY
		} MemoryState;

		explicit RtMeta(address_t addr):Meta(addr),racy(false),
			state(MEM_STATE_VIRGIN) {}
		~RtMeta() {}
		bool racy;
		MemoryState state;
		LockSet reader_lock_set;
		InstMap reader_inst_table;
		LockSet writer_lock_set;
		InstMap writer_inst_table;
		InstSet race_inst_set;
		ThreadSet thread_set;
		thread_t owner_thd_id;
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
		RtMeta *rt_meta);
	LockSet *GetWriterLockSet(thread_t curr_thd_id);
	LockSet *GetReaderLockSet(thread_t curr_thd_id);
	void MergeThreadSet(thread_t curr_thd_id,RtMeta *rt_meta);
	void MergeLockSet(thread_t curr_thd_id,RaceEventType race_event_type,
		RtMeta *rt_meta);
	void InitializeLockSet(thread_t curr_thd_id,RaceEventType race_event_type,
		RtMeta *rt_meta);
	DISALLOW_COPY_CONSTRUCTORS(RaceTrack);
};

}

#endif
