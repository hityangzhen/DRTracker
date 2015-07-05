#ifndef __RACE_THREAD_SANITIZER_H
#define __RACE_THREAD_SANITIZER_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include "core/segment_set.h"

#include <map>
#include <tr1/unordered_map>
#include <vector>

namespace race {

class ThreadSanitizer:public Detector {
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	typedef std::map<thread_t,Segment *> SegmentTable;
	typedef std::set<Segment *>SegmentSet;
	typedef std::map<thread_t,bool> CreateSegmentMap;
	typedef std::pair<thread_t,timestamp_t> Epoch;
	typedef std::map<address_t,std::vector<Epoch> > LockEpochMap;
	typedef std::tr1::unordered_map<address_t,uint32> LockReferenceMap;

	ThreadSanitizer();
	~ThreadSanitizer();

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
	void ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id);
	void AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
  								Inst *inst,thread_t childThdId);
	void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,thread_t child_thd_id);

	void BeforePthreadCondSignal(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
    void BeforePthreadCondBroadcast(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst, address_t addr);
    void BeforePthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
    	Inst *inst,address_t cond_addr, address_t mutex_addr);
    void AfterPthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
    	Inst *inst,address_t cond_addr, address_t mutex_addr);
    void BeforePthreadCondTimedwait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t cond_addr,address_t mutex_addr);
    void AfterPthreadCondTimedwait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t cond_addr,address_t mutex_addr);

    void BeforePthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
    	Inst *inst,address_t addr);
  	void AfterPthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
  		Inst *inst,address_t addr);

  	void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      	Inst *inst,address_t addr);
  	void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      	Inst *inst,address_t addr);

protected:
	//the meta class for memory access
	class ThreadSanitizerMeta:public Meta {
	public:
		typedef uint64 signature_t;
		typedef std::map<signature_t,Inst *>SignInstMap;
		typedef std::set<Inst *>InstSet;
		typedef std::set<signature_t> SignAccessSet;

		explicit ThreadSanitizerMeta(address_t a):Meta(a),racy(false)
		{}
		~ThreadSanitizerMeta() {};

		bool racy; //whether this meta is involved in any race
		SegmentSet writer_segment_set;
		SegmentSet reader_segment_set;
		SignInstMap writer_inst_table;
		SignInstMap reader_inst_table;
		SignAccessSet segment_write_access_set;
		SignAccessSet segment_read_access_set;

		InstSet race_inst_set;
	};

	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);
	void Racy(ThreadSanitizerMeta *thd_sani_meta,Inst *inst);
	//whether to track the racy inst
	bool track_racy_inst_;
	//
	LockSetTable writer_lock_set_table_;
	LockSetTable reader_lock_set_table_;
	//
	SegmentTable segment_table_;
	CreateSegmentMap create_segment_map_;
	//
	SegmentSet expired_segment_set_;
	LockEpochMap lock_epoch_map_;
	LockReferenceMap lock_reference_map_;

private:
	LockSet *GetWriterLockSet(thread_t curr_thd_id);
	LockSet *GetReaderLockSet(thread_t curr_thd_id);
	Segment *GetSegment(thread_t curr_thd_id);
	void SyncEvent(thread_t curr_thd_id) { curr_vc_map_[curr_thd_id]->Increment(curr_thd_id); }
	ThreadSanitizerMeta::signature_t SegmentSign(Segment *segment);
	void ClearExpiredSegments();

private:
	DISALLOW_COPY_CONSTRUCTORS(ThreadSanitizer);
};

}//namespace race

#endif