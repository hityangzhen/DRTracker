#ifndef __RACE_SIMPLE_LOCK_PLUS_H
#define __RACE_SIMPLE_LOCK_PLUS_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <set>
#include <vector>

namespace race {

class SimpleLockPlus:public Detector {
public:
	typedef std::map<thread_t,uint32> ThreadLockCountTable;
	SimpleLockPlus();
	~SimpleLockPlus();

	void Register();
	bool Enabled();
	void Setup(Mutex *lock,RaceDB *race_db);

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

protected:
	class SlpMeta:public Meta {
	public:
		typedef std::pair<timestamp_t,bool> ClockLockIszeroPair;
		typedef std::map<thread_t,ClockLockIszeroPair> ThreadClockLockIszeroPairMap;
		typedef std::set<Inst *> InstSet;
		typedef std::map<thread_t,Inst*> InstMap;

		explicit SlpMeta(address_t a):Meta(a),racy(false)
		{}
		~SlpMeta() {}

		ThreadClockLockIszeroPairMap writer_clip_map;
		ThreadClockLockIszeroPairMap reader_clip_map;

		InstMap writer_inst_table;
		InstMap reader_inst_table;
		bool racy;
		InstSet race_inst_set;
	};

	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);


	ThreadLockCountTable curr_lc_table_;
	//whether to track the racy inst
	bool track_racy_inst_;
private:
	DISALLOW_COPY_CONSTRUCTORS(SimpleLockPlus);
};

}//namespace race

#endif