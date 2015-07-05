#ifndef __RACE_SIMPLE_LOCK_H
#define __RACE_SIMPLE_LOCK_H

#include "core/basictypes.h"
#include "race/detector.h"
#include "race/race.h"
#include <set>
#include <vector>


namespace race {
class SimpleLock:public Detector {
public:
	typedef std::map<thread_t,uint32> ThreadLockCountTable;
	
	SimpleLock();
	~SimpleLock();

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
	class SlMeta:public Meta {
	public:
		typedef std::set<Inst *> InstSet;
		typedef std::vector<Inst *> InstList;
		typedef std::map<thread_t,InstList *> InstListMap;

		typedef std::pair<timestamp_t,uint32> ClockLockcntPair;
		typedef std::vector<ClockLockcntPair> ClockLockcntPairList;
		typedef std::map<thread_t,ClockLockcntPairList *> ThreadClockLockcntPairListMap;

		explicit SlMeta(address_t a):Meta(a),racy(false) 
		{}
		~SlMeta() {}

		bool racy;
		//store inst list corresponding to clock lockcnt list
		InstListMap writer_instlist_table;
		InstListMap reader_instlist_table;
		InstSet race_inst_set;

		ThreadClockLockcntPairListMap reader_clpl_map;
		ThreadClockLockcntPairListMap writer_clpl_map;

		static void SetMaxListLen(uint32 len) {
			max_list_len=len;
		}
		static uint32 max_list_len;
	};

	
	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);
	
	ThreadLockCountTable curr_lc_table_;
	bool track_racy_inst_;
private:
	DISALLOW_COPY_CONSTRUCTORS(SimpleLock);
};

}//namespace race

#endif