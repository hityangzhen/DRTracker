#ifndef __RACE_VERIFIER_H
#define __RACE_VERIFIER_H
#include <tr1/unordered_map>
#include <ctime>
#include "race/potential_race.h"
#include "core/basictypes.h"
#include "core/filter.h"
#include "core/analyzer.h"
#include "race/race.h"
#include "core/pin_sync.hpp"
#include "core/log.h"

namespace race 
{

#define random(x) (rand()%x)
#define READ 0
#define WRITE 1

#define MAP_KEY_NOTFOUND_NEW(map,key,value_type) \
	if((map).find(key)==(map).end() || (map)[key]==NULL) \
		(map)[key]=new value_type

class Verifier:public Analyzer {
protected:
	//data for memory unit
	class Meta {
	public:
		typedef std::tr1::unordered_map<address_t,Meta *> Table;
		explicit Meta(address_t a):addr(a),readers(0),writers(0) {}
		virtual ~Meta() {}

		void AddReader() { readers<0?readers=1:readers+=1; }
		void AddWriter() { writers<0?writers=1:writers+=1; }
		uint16 GetReaders() { return readers; }
		uint16 GetWriters() { return writers; }

		void CutReader() { readers-=1; }
		void CutWriter() { writers-=1; }

 		address_t addr;
		uint16 readers;
		uint16 writers;
	};

	//data for mutex 
	class MutexMeta {
	public:
		typedef std::tr1::unordered_map<address_t,MutexMeta *> Table;
		MutexMeta() : thd_id(0){}
		~MutexMeta() {}
		thread_t GetOwner() {
			return thd_id;
		}
		void SetOwner(thread_t t) {
			thd_id=t;
		}
		thread_t thd_id;
	};

	enum Status {
		INITIAL,
		POSTPONED,
		AVAILABLE,
		TERMINAL
	};
public:
	typedef std::set<thread_t> PostponeThreadSet;
	typedef std::tr1::unordered_set<Meta *> MetaSet;
	typedef std::tr1::unordered_map<thread_t,MetaSet *> ThreadMetasMap;
	typedef std::map<thread_t,SysSemaphore *> ThreadSemaphoreMap;
	typedef std::set<thread_t> BlockThreadSet;
	typedef std::set<thread_t> AvailableThreadSet;
	typedef std::map<PStmt *,MetaSet *> PStmtMetasMap;
	typedef std::set<PStmt *> PStmtSet;

	Verifier();
	virtual ~Verifier();

	virtual void Register();
	virtual void Setup(Mutex *internal_lock,Mutex *verify_lock,PRaceDB *prace_db);
	virtual bool Enabled();

	//image load-unload
	virtual void ImageLoad(Image *image,address_t low_addr,address_t high_addr,
		address_t data_start,size_t data_size,address_t bss_start,size_t bss_size);
	virtual void ImageUnload(Image *image,address_t low_addr,address_t high_addr,
		address_t data_start,size_t data_size,address_t bss_start,size_t bss_size);
	//thread start-exit
	virtual void ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id);
	virtual void ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk);

	//thread create-join
	virtual void BeforePthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,thread_t child_thd_id);
	virtual void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,thread_t child_thd_id);

	//read-write
	virtual void BeforeMemRead(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t addr,size_t size);
	virtual void BeforeMemWrite(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t addr,size_t size);

	//malloc-free
	virtual void AfterMalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    	Inst *inst, size_t size, address_t addr);
  	virtual void AfterCalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    	Inst *inst, size_t nmemb, size_t size,address_t addr);
  	virtual void BeforeRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
   		Inst *inst, address_t ori_addr, size_t size);
  	virtual void AfterRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    	Inst *inst, address_t ori_addr, size_t size,address_t new_addr);
  	virtual void BeforeFree(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    	Inst *inst, address_t addr);

  	//mutex-rwlock
  	virtual void BeforePthreadMutexTryLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadMutexTryLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr,int retVal);
	virtual void BeforePthreadMutexLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadMutexLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void BeforePthreadMutexUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadMutexUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
private:

	void AllocAddrRegion(address_t addr,size_t size);
	void FreeAddrRegion(address_t addr);
	//we need to use the filter internal lock
	bool FilterAccess(address_t addr) {return filter_->Filter(addr,true);}
	void Process(thread_t curr_thd_id,address_t addr,Inst *inst,bool is_read);
	Meta* GetMeta(address_t addr);

	thread_t RandomThread(std::set<thread_t>&thd_set) {
		srand((unsigned)time(NULL));
		//simply iterate
		if(thd_set.size()==1)
			return *thd_set.begin();
		int i=0,val=random(thd_set.size());
		std::set<thread_t>::iterator iter;
		for(iter=thd_set.begin();i<val;iter++,i++)
			;		
		return *iter;
	}
	bool RandomBool() {
		srand((unsigned)time(NULL));
		return random(2)==0?false:true;
	}

	void VerifyLock() {verify_lock_->Lock();}
	void VerifyUnlock() {verify_lock_->Unlock();}
	void InternalLock() {internal_lock_->Lock();}
	void InternalUnlock() {internal_lock_->Unlock();}

	void PrintDebugRaceInfo(Meta *meta,RaceType race_type,Inst *inst);
	
	void ChooseRandomThreadBeforeExecute(address_t addr,thread_t curr_thd_id);
	void ChooseRandomThreadAfterAllUnavailable();
	void ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
		size_t size,bool is_read);
	void RacedMeta(PStmt *first_pstmt,address_t start_addr,address_t end_addr,
		PStmt *second_pstmt,Inst *inst,thread_t curr_thd_id,bool is_read,
		MetaSet &raced_metas);
	void WakeUpPostponeThreadSet(PostponeThreadSet *pp_thds);
	void WakeUpPostponeThread(thread_t thd_id);
	void PostponeThread(thread_t curr_thd_id);
	void HandleRace(PostponeThreadSet *pp_thds,thread_t curr_thd_id);
	void HandleNoRace(thread_t curr_thd_id);

	void ClearPStmtCorrespondingMetas(PStmt *pstmt,MetaSet *metas);
	//need to be protected
	void BlockThread(thread_t curr_thd_id) {
		blk_thd_set_.insert(curr_thd_id);
		avail_thd_set_.erase(curr_thd_id);
	}
	void UnblockThread(thread_t curr_thd_id) {
		blk_thd_set_.erase(curr_thd_id);
		avail_thd_set_.insert(curr_thd_id);
	}
	Mutex *internal_lock_;
	Mutex *verify_lock_;
	PRaceDB *prace_db_;
	RegionFilter *filter_;

	address_t unit_size_;
	//random thread id
	thread_t rdm_thd_id_;

	Meta::Table meta_table_;
	MutexMeta::Table mutex_meta_table_;

	//pospone thread set
	PostponeThreadSet pp_thd_set_;
	//available thread set
	AvailableThreadSet avail_thd_set_;
	//blocked thread set
	BlockThreadSet blk_thd_set_;
	//metas belong to accessed pstmt
	PStmtMetasMap pstmt_metas_map_;
	//thread accessed metas
	ThreadMetasMap thd_metas_map_;
	//thread corresponding semaphore
	ThreadSemaphoreMap thd_smp_map_;
};

}// namespace race

#endif //__RACE_VERIFIER_H