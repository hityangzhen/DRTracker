#ifndef __RACE_VERIFIER_H
#define __RACE_VERIFIER_H
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <ctime>
#include "race/potential_race.h"
#include "core/basictypes.h"
#include "core/filter.h"
#include "core/analyzer.h"
#include "race/race.h"
#include "core/pin_sync.hpp"
#include "core/vector_clock.h"

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
	//meta access snapshot
	struct MetaSnapshot {
		MetaSnapshot(timestamp_t clk,RaceEventType t,Inst *i):thd_clk(clk),type(t),
		inst(i) {}
		timestamp_t thd_clk;
		RaceEventType type;
		Inst *inst;
	};
	typedef std::vector<MetaSnapshot> MetaSnapshotVector;
	typedef std::tr1::unordered_map<thread_t,MetaSnapshotVector *> MetaSnapshotsMap;
	//data for memory unit
	class Meta {
	public:
		typedef std::tr1::unordered_map<address_t,Meta *> Table;
		typedef std::tr1::unordered_set<Inst *> InstSet;
		explicit Meta(address_t a):addr(a) {}
		virtual ~Meta() {
			for(MetaSnapshotsMap::iterator iter=meta_ss_map.begin();
				iter!=meta_ss_map.end();iter++)
				delete iter->second;
		}
		void AddMetaSnapshot(thread_t thd_id,MetaSnapshot meta_ss) {
			if(meta_ss_map.find(thd_id)==meta_ss_map.end())
				meta_ss_map[thd_id]=new MetaSnapshotVector;
			meta_ss_map[thd_id]->push_back(meta_ss);
		}

		void AddRacedInstPair(Inst *i1,Inst *i2) {
			raced_inst_set.insert(i1);
			raced_inst_set.insert(i2);
		}
		bool RacedInstPair(Inst *i1,Inst *i2) {
			return raced_inst_set.find(i1)!=raced_inst_set.end() &&
					raced_inst_set.find(i2)!=raced_inst_set.end();
		}

		address_t addr;
		InstSet raced_inst_set;
		MetaSnapshotsMap meta_ss_map;
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
		VectorClock vc;
	};

	class RwlockMeta {
	public:
		typedef std::tr1::unordered_map<address_t,RwlockMeta *> Table;
		typedef std::set<thread_t> ThreadSet; 
		RwlockMeta():wrlock_thd_id(0),ref(0) {}
		~RwlockMeta() {}
		void AddRdlockOwner(thread_t t) { rdlock_thd_set.insert(t); }
		void RemoveRdlockOwner(thread_t t) { rdlock_thd_set.erase(t); }
		ThreadSet *GetRdlockOwners() { return &rdlock_thd_set; }
		bool HasRdlockOwner() { return !rdlock_thd_set.empty(); }
		void SetWrlockOwner(thread_t t) { wrlock_thd_id=t; }
		thread_t GetWrlockOwner() { return wrlock_thd_id; }
		ThreadSet rdlock_thd_set;
		thread_t wrlock_thd_id;

		VectorClock vc;
		VectorClock wait_vc;
		uint8 ref;
	};

	class BarrierMeta {
	public:
		typedef std::tr1::unordered_map<address_t,BarrierMeta *> Table;
		BarrierMeta():count(0),ref(0) {}
		~BarrierMeta() {}
		VectorClock vc;
		uint8 count;
		uint8 ref;
	};

	class CondMeta {
	public:
		typedef std::tr1::unordered_map<address_t,CondMeta *> Table;
		typedef std::map<thread_t,VectorClock> ThreadVectorClockMap;
		CondMeta() {}
		~CondMeta() {}

		ThreadVectorClockMap wait_table;
		ThreadVectorClockMap signal_table;
	};

	class SemMeta {
	public:
		typedef std::tr1::unordered_map<address_t,SemMeta *> Table;
		SemMeta():count(0) {}
		~SemMeta() {}
		VectorClock vc;
		uint8 count;
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
	typedef std::tr1::unordered_map<thread_t,VectorClock *> ThreadVectorClockMap;

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
	virtual void AfterPthreadCreate(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,thread_t child_thd_id);
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
		timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val);
	virtual void BeforePthreadMutexLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadMutexLock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void BeforePthreadMutexUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadMutexUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);

	//read-write lock
	virtual void BeforePthreadRwlockRdlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockRdlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void BeforePthreadRwlockWrlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockWrlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void BeforePthreadRwlockUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockUnlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void BeforePthreadRwlockTryRdlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val);
	virtual void BeforePthreadRwlockTryWrlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr);
	virtual void AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,
		timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val);

	//barrier
  	virtual void BeforePthreadBarrierWait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  	virtual void AfterPthreadBarrierWait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  	virtual void AfterPthreadBarrierInit(thread_t curr_thd_id,
  		timestamp_t curr_thd_clk, Inst *inst,address_t addr, unsigned int count);

  	//condition-variable
  	virtual void BeforePthreadCondSignal(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t addr);
  	virtual void BeforePthreadCondBroadcast(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk,Inst *inst, address_t addr);
  	virtual void BeforePthreadCondWait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    	address_t mutex_addr);
  	virtual void AfterPthreadCondWait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr, 
    	address_t mutex_addr);
  	virtual void BeforePthreadCondTimedwait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    	address_t mutex_addr);
  	virtual void AfterPthreadCondTimedwait(thread_t curr_thd_id,
    	timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    	address_t mutex_addr);

  	//semaphore
  	virtual void AfterSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr,unsigned int value);
  	virtual void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t addr);
  	virtual void BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,address_t addr);
  	virtual void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      	Inst *inst,address_t addr);
private:

	void AllocAddrRegion(address_t addr,size_t size);
	void FreeAddrRegion(address_t addr);
	//we need to use the filter internal lock
	bool FilterAccess(address_t addr) {return filter_->Filter(addr,true);}
	void Process(thread_t curr_thd_id,address_t addr,Inst *inst,RaceEventType type);

	Meta* GetMeta(address_t addr);
	void ProcessFree(Meta *meta);

  	MutexMeta *GetMutexMeta(address_t addr);
  	void ProcessPreMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessPostMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessPreMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessPostMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessFree(MutexMeta *mutex_meta);

  	RwlockMeta *GetRwlockMeta(address_t addr);
  	void ProcessFree(RwlockMeta *rwlock_meta);

  	BarrierMeta *GetBarrierMeta(address_t addr);
  	void ProcessFree(BarrierMeta *barrier_meta);

  	CondMeta *GetCondMeta(address_t addr);
  	void ProcessSignal(thread_t curr_thd_id,CondMeta *cond_meta);
  	void ProcessFree(CondMeta *cond_meta);

  	SemMeta *GetSemMeta(address_t addr);
  	void ProcessFree(SemMeta *sem_meta);
		
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

	void PrintDebugRaceInfo(Meta *meta,RaceType race_type,thread_t t1,Inst *i1,
		thread_t t2,Inst *i2);
	
	void ChooseRandomThreadBeforeExecute(address_t addr,thread_t curr_thd_id);
	void ChooseRandomThreadAfterAllUnavailable();
	void ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
		size_t size,RaceEventType type);
	void RacedMeta(PStmt *first_pstmt,address_t start_addr,address_t end_addr,
		PStmt *second_pstmt,Inst *inst,thread_t curr_thd_id,RaceEventType type,
		PostponeThreadSet &pp_thds);
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
	RwlockMeta::Table rwlock_meta_table_;
	BarrierMeta::Table barrier_meta_table_;
	CondMeta::Table cond_meta_table_;
	SemMeta::Table sem_meta_table_;

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
	//
	ThreadVectorClockMap thd_vc_map_;
};

}// namespace race

#endif //__RACE_VERIFIER_H