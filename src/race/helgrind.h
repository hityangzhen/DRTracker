#ifndef __RACE_HELGRIND_H
#define __RACE_HELGRIND_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <map>
#include <set>
#include <tr1/unordered_map>

namespace race {

#define SEGMENT_NEW 0
#define SEGMENT_FIRST_READ 1
#define SEGMENT_FIRST_WRITE 2

class Helgrind:public Detector{
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	typedef std::map<thread_t,char> CreateSegmentMap;
	typedef std::tr1::unordered_map<address_t,uint32> LockReferenceMap;
	typedef std::map<thread_t,RaceEventType> LastopTable;
	Helgrind();
	~Helgrind();

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
	//simple segment
	class Segment {
	public:
		Segment() {}
		Segment(VectorClock &v,thread_t t,RaceEventType r)
		:vc(v),thd_id(t),race_event_type(r) {}
		VectorClock vc;
		thread_t thd_id;
		RaceEventType race_event_type;
		bool HappensBefore(Segment &s) { return vc.HappensBefore(&s.vc); }
		bool Concurrent(Segment &s) {
			return !vc.PreciseHappensBefore(&s.vc) && !s.vc.PreciseHappensBefore(&vc);
		}
	};

	class HgMeta:public Meta {
	public:
		typedef enum {
			MEM_STATE_NEW,
			MEM_STATE_EXCLUSIVE_READ,
			MEM_STATE_EXCLUSIVE_WRITE,
			MEM_STATE_SHARED_READ,
			MEM_STATE_SHARED_MODIFIED,
			MEM_STATE_RACE
		} MemoryState;
		typedef std::set<thread_t> ThreadSet;
		typedef std::set<Inst *> InstSet;
		explicit HgMeta(address_t a):Meta(a),racy(false),state(MEM_STATE_NEW) {}
		~HgMeta() {}

		void UpdateSegment(VectorClock &v,thread_t t,RaceEventType r) {
			segment.vc=v;
			segment.thd_id=t;
			segment.race_event_type=r;
		}

		bool racy;
		MemoryState state;
		Segment segment;
		Inst *inst;
		InstSet race_inst_set;
		ThreadSet thread_set;
		LockSet lock_set;
		LastopTable lastop_table;
	};

	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t cutt_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);

	bool track_racy_inst_;
	LockSetTable writer_lock_set_table_;
	LockSetTable reader_lock_set_table_;
	CreateSegmentMap create_segment_map_;
	LockReferenceMap lock_reference_map_;
private:
	void UpdateMemoryState(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,Inst *inst);
	void ChangeStateExclusiveRead(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst);
	void ChangeStateExclusiveWrite(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst);
	void ChangeStateSharedRead(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst);
	void ChangeStateSharedModified(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst);
	void ChangeStateNew(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst);
	void MergeLockSet(thread_t curr_thd_id,RaceEventType race_event_type,
		HgMeta *hg_meta);
	LockSet *GetWriterLockSet(thread_t curr_thd_id);
	LockSet *GetReaderLockSet(thread_t curr_thd_id);

	DISALLOW_COPY_CONSTRUCTORS(Helgrind);
};

}

#endif