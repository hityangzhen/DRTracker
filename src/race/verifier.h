#ifndef __RACE_VERIFIER_H
#define __RACE_VERIFIER_H
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <ctime>
#include <deque>
#include "race/potential_race.h"
#include "core/basictypes.h"
#include "core/filter.h"
#include "core/analyzer.h"
#include "race/race.h"
#include "core/pin_sync.hpp"
#include "core/vector_clock.h"
#include "race/loop.h"
#include "race/cond_wait.h"

//default dynamic data race detection engine is FastTrack

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
	//limit the snapshot vector length
	static size_t ss_deq_len; 
	//meta access snapshot
	class MetaSnapshot {
	public:
		MetaSnapshot(timestamp_t clk,RaceEventType t,Inst *i,PStmt *s):thd_clk(clk),type(t),
			inst(i),pstmt(s) {}
		virtual ~MetaSnapshot() {}
		timestamp_t thd_clk;
		RaceEventType type;
		Inst *inst;
		PStmt *pstmt;
	};
	typedef std::deque<MetaSnapshot *> MetaSnapshotDeque;
	typedef std::tr1::unordered_map<thread_t,MetaSnapshotDeque *> MetaSnapshotsMap;
	//data for memory unit
	class Meta {
	public:
		typedef std::tr1::unordered_map<address_t,Meta *> Table;
		typedef std::tr1::unordered_set<uint64> RacedInstPairSet;
		explicit Meta(address_t a):addr(a) {
			if(Verifier::unit_size_!=0) {
				lastest_values=new char[Verifier::unit_size_];
				for(address_t iaddr=0;iaddr!=Verifier::unit_size_;iaddr++) {
					lastest_values[iaddr]=*((char *)addr+iaddr);
				}
			}
		}
		virtual ~Meta() {
			for(MetaSnapshotsMap::iterator iter=meta_ss_map.begin();
				iter!=meta_ss_map.end();iter++) {
				//clear all meta snapshots
				MetaSnapshotDeque *meta_ss_deq=iter->second;
				for(MetaSnapshotDeque::iterator iiter=meta_ss_deq->begin();
					iiter!=meta_ss_deq->end();iiter++)
					delete *iiter;
				delete meta_ss_deq;
			}
			//clear the value
			delete []lastest_values;
		}
		void AddRacedInstPair(Inst *i1,Inst *i2) {
			uint64 x=reinterpret_cast<uint64>(i1);
			uint64 y=reinterpret_cast<uint64>(i2);
			raced_inst_pair_set.insert(x+(y<<32));
			raced_inst_pair_set.insert(y+(x<<32));
		}
		bool RacedInstPair(Inst *i1,Inst *i2) {
			uint64 x=reinterpret_cast<uint64>(i1);
			uint64 y=reinterpret_cast<uint64>(i2);
			return raced_inst_pair_set.find(x+(y<<32))!=raced_inst_pair_set.end();
		}
		void AddMetaSnapshot(thread_t thd_id,MetaSnapshot *meta_ss) {
			if(meta_ss_map.find(thd_id)==meta_ss_map.end())
				meta_ss_map[thd_id]=new MetaSnapshotDeque;
			//too many snapshots
			if(meta_ss_map[thd_id]->size()==Verifier::ss_deq_len) {
				MetaSnapshot *meta_ss=meta_ss_map[thd_id]->front();
				meta_ss_map[thd_id]->pop_front();
				delete meta_ss;
			}
			meta_ss_map[thd_id]->push_back(meta_ss);
		}
		MetaSnapshot* LastestMetaSnapshot(thread_t thd_id) {
			if(meta_ss_map.find(thd_id)==meta_ss_map.end() || 
				meta_ss_map[thd_id]->size()==0)
				return NULL;
			return meta_ss_map[thd_id]->back();
		}
		bool RecentMetaSnapshot(thread_t thd_id,timestamp_t thd_clk,Inst *inst) {
			if(meta_ss_map.find(thd_id)==meta_ss_map.end() ||
				meta_ss_map[thd_id]->size()==0)
				return false;
			//munally set the previous step is 3;
			MetaSnapshotDeque *meta_ss_deq=meta_ss_map[thd_id];
			size_t step=meta_ss_deq->size()<3?meta_ss_deq->size():3;

			for(MetaSnapshotDeque::reverse_iterator iter=meta_ss_deq->rbegin();
				iter!=meta_ss_deq->rbegin()+step;++iter) {
				if((*iter)->inst==inst && (*iter)->thd_clk==thd_clk)
					return true;
			}
			return false;
		}
		void ResetLastestValues() {
			for(address_t iaddr=0;iaddr!=Verifier::unit_size_;iaddr++) {
				lastest_values[iaddr]=*((char *)addr+iaddr);
			}
		}
		char *Lastestvalues() { return lastest_values; }
		//record the lastest values
		char *lastest_values;
		address_t addr;
		RacedInstPairSet raced_inst_pair_set;
		MetaSnapshotsMap meta_ss_map;
	};

	//data for mutex 
	class MutexMeta {
	public:
		typedef std::tr1::unordered_map<address_t,MutexMeta *> Table;
		MutexMeta(address_t a) : addr(a),thd_id(0) {}
		~MutexMeta() {}
		thread_t GetOwner() {
			return thd_id;
		}
		void SetOwner(thread_t t) {
			thd_id=t;
		}
		address_t addr;
		thread_t thd_id;
		VectorClock vc;
	};

	class RwlockMeta {
	public:
		typedef std::tr1::unordered_map<address_t,RwlockMeta *> Table;
		typedef std::set<thread_t> ThreadSet; 
		RwlockMeta(address_t a) : addr(a),wrlock_thd_id(0),ref(0) {}
		~RwlockMeta() {}
		void AddRdlockOwner(thread_t t) { rdlock_thd_set.insert(t); }
		void RemoveRdlockOwner(thread_t t) { rdlock_thd_set.erase(t); }
		ThreadSet *GetRdlockOwners() { return &rdlock_thd_set; }
		bool HasRdlockOwner() { return !rdlock_thd_set.empty(); }
		void SetWrlockOwner(thread_t t) { wrlock_thd_id=t; }
		thread_t GetWrlockOwner() { return wrlock_thd_id; }
		address_t addr;
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
		int8 count; //should consider negative
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
	virtual void Setup(Mutex *internal_lock,Mutex *verify_lock,RaceDB *race_db,
		PRaceDB *prace_db);
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

	//call-return
	virtual void BeforeCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t target);
	virtual void AfterCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t target,address_t ret);
	virtual void BeforeReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t target);
	virtual void AfterReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t target);

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
protected:

	void AllocAddrRegion(address_t addr,size_t size);
	void FreeAddrRegion(address_t addr);
	//we need to use the filter internal lock
	bool FilterAccess(address_t addr) {return filter_->Filter(addr,true);}
	void Process(thread_t curr_thd_id,address_t addr,Inst *inst,RaceEventType type);

	virtual Meta* GetMeta(address_t addr);
	void ProcessFree(Meta *meta);

  	MutexMeta *GetMutexMeta(address_t addr);
  	void ProcessPreMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	virtual void ProcessPostMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	virtual void ProcessPreMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessPostMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);
  	void ProcessFree(MutexMeta *mutex_meta);

  	RwlockMeta *GetRwlockMeta(address_t addr);
  	void ProcessPreRwlockRdlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	virtual void ProcessPostRwlockRdlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	void ProcessPreRwlockWrlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	virtual void ProcessPostRwlockWrlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	virtual void ProcessPreRwlockUnlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	void ProcessPostRwlockUnlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
  	void ProcessFree(RwlockMeta *rwlock_meta);

  	BarrierMeta *GetBarrierMeta(address_t addr);
  	void ProcessFree(BarrierMeta *barrier_meta);

  	CondMeta *GetCondMeta(address_t addr);
  	void ProcessSignal(thread_t curr_thd_id,CondMeta *cond_meta);
  	void ProcessFree(CondMeta *cond_meta);

  	SemMeta *GetSemMeta(address_t addr);
  	void ProcessFree(SemMeta *sem_meta);
		
	thread_t RandomThread(std::set<thread_t>&thd_set) {
		if(thd_set.empty())
			return 0;
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
	void ReportRace(Meta *meta, thread_t t0, Inst *i0,RaceEventType p0, 
		thread_t t1, Inst *i1,RaceEventType p1) {
		race_db_->CreateRace(meta->addr,t0,i0,p0,t1,i1,p1,false);
	}
	
	void ChooseRandomThreadBeforeExecute(address_t addr,thread_t curr_thd_id);
	void ChooseRandomThreadAfterAllUnavailable();
	void ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
		size_t size,RaceEventType type);
	void RacedMeta(PStmt *first_pstmt,address_t start_addr,address_t end_addr,
		PStmt *second_pstmt,Inst *inst,thread_t curr_thd_id,RaceEventType type,
		std::map<thread_t,bool> &pp_thd_map);
	//wrapper function
	virtual void AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
		timestamp_t curr_thd_clk,RaceEventType type,Inst *inst,PStmt *s) {
		MetaSnapshot *meta_ss=new MetaSnapshot(curr_thd_clk,type,inst,s);
		meta->AddMetaSnapshot(curr_thd_id,meta_ss);
	}
	//only consider timestamp,curr_type is unused
	virtual RaceType HistoryRace(MetaSnapshot *meta_ss,thread_t thd_id,
		thread_t curr_thd_id,RaceEventType curr_type) {
		timestamp_t thd_clk=thd_vc_map_[curr_thd_id]->GetClock(thd_id);
		if(meta_ss->thd_clk > thd_clk) {
			if(curr_type==RACE_EVENT_WRITE)
				return meta_ss->type==RACE_EVENT_WRITE ? 
					WRITETOWRITE : READTOWRITE;
			else if(meta_ss->type==RACE_EVENT_WRITE)
				return WRITETOREAD;
		}
		return NONE;
	}
	void WakeUpPostponeThreadSet(PostponeThreadSet &pp_thds);
	void WakeUpPostponeThread(thread_t thd_id);
	void PostponeThread(thread_t curr_thd_id);
	void KeepupThread(thread_t curr_thd_id);
	void HandleRace(std::map<thread_t,bool> &pp_thd_map,thread_t curr_thd_id);
	void HandleNoRace(thread_t curr_thd_id);

	void ClearPostponedThreadMetas(thread_t curr_thd_id);
	void ClearPStmtCorrespondingMetas(PStmt *pstmt,MetaSet *metas);

	//need to be protected by sync
	void BlockThread(thread_t curr_thd_id) {
		blk_thd_set_.insert(curr_thd_id);
		avail_thd_set_.erase(curr_thd_id);
	}
	void UnblockThread(thread_t curr_thd_id) {
		blk_thd_set_.erase(curr_thd_id);
		avail_thd_set_.insert(curr_thd_id);
	}
	//ad-hoc wrapper functions
	void ProcessWriteReadSync(thread_t curr_thd_id,Inst *curr_inst);
	void WakeUpAfterProcessWriteReadSync(thread_t curr_thd_id,Inst *curr_inst);
	//cond_wait wrapper functions
	void ProcessLockSignalWrite(thread_t curr_thd_id,address_t addr);
	bool ProcessCondWaitRead(thread_t curr_thd_id,Inst *curr_inst,
		address_t addr);
	bool ProcessCondWaitRead(thread_t curr_thd_id,Inst *curr_inst,
		address_t addr,std::string &file_name,int line);
	void ProcessSignalCondWaitSync(thread_t curr_thd_id,Inst *curr_inst);

	Mutex *internal_lock_;
	Mutex *verify_lock_;
	RaceDB *race_db_;
	PRaceDB *prace_db_;
	RegionFilter *filter_;

	static address_t unit_size_;
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
	//thread accessed metas-history metas
	ThreadMetasMap thd_metas_map_;
	//postponed threads' metas
	ThreadMetasMap thd_ppmetas_map_;
	//thread corresponding semaphore
	ThreadSemaphoreMap thd_smp_map_;
	//thread corresponding vector clock
	ThreadVectorClockMap thd_vc_map_;
	//loop info
	LoopDB *loop_db_;
	//cond_wait info
	CondWaitDB *cond_wait_db_;
private:
	DISALLOW_COPY_CONSTRUCTORS(Verifier);
};

}// namespace race

#endif //__RACE_VERIFIER_H