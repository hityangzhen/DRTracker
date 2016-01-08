#ifndef __PRE_GROUP_H
#define __PRE_GROUP_H

//record the logical time of stmt to make preparation for group

//record the logical 
#include "race/potential_race.h"
#include "core/vector_clock.h"
#include "race/detector.h"
#include <map>
namespace race
{
class PreGroup:public Detector {
public:
	typedef std::map<thread_t,uint64> ThreadExecCountMap;
	PreGroup();
	~PreGroup();
	void Register();
	bool Enabled();
	void Setup(Mutex *lock,PRaceDB *prace_db);

	void ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id);
	void BeforeMemRead(thread_t curr_thd_id, timestamp_t curr_thd_clk,
		Inst *inst, address_t addr, size_t size);
	void BeforeMemWrite(thread_t curr_thd_id, timestamp_t curr_thd_clk,
		Inst *inst, address_t addr, size_t size);
	void BeforeAtomicInst(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,std::string type, address_t addr);
	void Export();
protected:
	virtual void ProcessLock(thread_t curr_thd_id,MutexMeta *meta) { }
	virtual void ProcessUnlock(thread_t curr_thd_id,MutexMeta *meta) { }
	//trivial 
	virtual Meta *GetMeta(address_t iaddr) { return NULL; }
	virtual void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst) { }
	virtual void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst) { }
	virtual void ProcessFree(Meta *meta) { }

	void RecordPStmtLogicalTime(thread_t curr_thd_id,Inst *inst);

	ThreadExecCountMap thd_ec_map_;
	PRaceDB *prace_db_;
private:
	DISALLOW_COPY_CONSTRUCTORS(PreGroup);
};
} //namespace race
#endif //__PRE_GROUP_H