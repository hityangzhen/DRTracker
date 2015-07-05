#ifndef __RACE_FAST_TRACK_H
#define __RACE_FAST_TRACK_H

//FastTrack algorithm

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <set>


namespace race {

class FastTrack:public Detector {
public:
	
	typedef std::pair<thread_t,timestamp_t> Epoch;
	FastTrack();
	virtual ~FastTrack();

	virtual void Register();
	virtual bool Enabled();
	virtual void Setup(Mutex *lock,RaceDB *race_db);

	virtual void AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
	virtual void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,thread_t child_thd_id);

	virtual void AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
	virtual void BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
protected:
	//the meta data for memory
	class FtMeta:public Meta {
	public:
		typedef std::set<Inst *> InstSet;
		typedef std::map<thread_t,Inst*> InstMap;

		explicit FtMeta(address_t a):Meta(a),shared(false),racy(false),
		writer_epoch(Epoch(0,0)),reader_epoch(Epoch(0,0))
		{}
		~FtMeta() {}
		VectorClock reader_vc;
		InstMap writer_inst_table;
		InstMap reader_inst_table;
		bool shared;
		bool racy;
		Epoch writer_epoch;
		Epoch reader_epoch;
		InstSet race_inst_set;
	};

	class RwlockMeta:public MutexMeta {
	public:
		RwlockMeta():ref_count(0) {}
		virtual ~RwlockMeta() {}
		int ref_count;
		VectorClock wait_vc;
	};

	//override virtual function
	virtual Meta *GetMeta(address_t iaddr);
	virtual void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst);
	virtual void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst);
	virtual void ProcessFree(Meta *meta);

	virtual MutexMeta *GetMutexMeta(address_t addr);
	//whether to track the racy inst
	bool track_racy_inst_;
private:
	DISALLOW_COPY_CONSTRUCTORS(FastTrack);
};

} //namespace race

#endif /* __RACE_FAST_TRACK_H */