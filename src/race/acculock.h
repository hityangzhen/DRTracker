#ifndef __RACE_ACCULOCK_H
#define __RACE_ACCULOCK_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <set>

namespace race{

class AccuLock:public Detector {
public:
	typedef std::map<thread_t,LockSet *> LockSetTable;
	
	AccuLock();
	~AccuLock();

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
	class AccuLockMeta:public Meta {
	public:
		typedef std::pair<thread_t,timestamp_t> Epoch;
		typedef std::pair<Epoch,LockSet> EpochLockSetPair;
		typedef std::map<thread_t,EpochLockSetPair> ThreadEpLsMap;
		typedef std::set<Inst *> InstSet;
		typedef std::map<thread_t,Inst*> InstMap;

		explicit AccuLockMeta(address_t a):Meta(a),racy(false) {
			writer_epls_pair.first.first=writer_epls_pair.first.second=0;
		}
		~AccuLockMeta(){}

		ThreadEpLsMap reader_epls_map;
		EpochLockSetPair writer_epls_pair;
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
	//locks having been protected current write accesses
	LockSetTable curr_lockset_table_;
	//locks having been proteced current read accesses
	LockSetTable curr_reader_lockset_table_;
private:
	DISALLOW_COPY_CONSTRUCTORS(AccuLock);
};

}//namespace race

#endif