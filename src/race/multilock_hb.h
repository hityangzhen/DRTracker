#ifndef __RACE_MULTILOCK_HB_H
#define __RACE_MULTILOCK_HB_H

//this detector is enhanced at the basic of acculock

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <set>

namespace race {

class MultiLockHb:public Detector {
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	MultiLockHb();
	~MultiLockHb();

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
	class MlMeta:public Meta {
	public:
		typedef std::pair<timestamp_t,LockSet> EpochLockSetPair;
		typedef std::vector<EpochLockSetPair *> EpochLockSetPairVector; 
		typedef std::map<thread_t,EpochLockSetPairVector *> ThreadElspVecMap;
		typedef std::set<Inst *> InstSet;
		typedef std::map<thread_t,Inst*> InstMap;

		explicit MlMeta(address_t a):Meta(a),racy(false) {}
		~MlMeta() {}

		ThreadElspVecMap reader_elspvec_map;
		ThreadElspVecMap writer_elspvec_map;
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

	//whether to track the racy inst
	bool track_racy_inst_;
	LockSetTable curr_lockset_table_;
	LockSetTable curr_reader_lockset_table_;
private:
	void update_on_read(timestamp_t curr_clk,thread_t curr_thd,LockSet *curr_lockset,
		MlMeta *ml_meta);
	void update_on_write(timestamp_t curr_clk,thread_t curr_thd,LockSet *curr_lockset,
		MlMeta *ml_meta);
	DISALLOW_COPY_CONSTRUCTORS(MultiLockHb);
};

}//namespace race

#endif 