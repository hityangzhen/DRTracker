#ifndef __RACE_COND_WAIT_H
#define __RACE_COND_WAIT_H

//Use the default fasttrack dynamic data race detection engine to
//identify signal/broadcast->cond_wait sync

#include <tr1/unordered_set>
#include <tr1/unordered_map>
#include <map>
#include <set>
#include <deque>
#include "core/basictypes.h"
#include "core/vector_clock.h"
#include "race/loop.h"
#include "race.h"

namespace race
{
#define WRITE_META_LEN 10

class CondWaitLoop:public Loop {
public:
	CondWaitLoop(int sl,int el):Loop(sl,el) {}
	~CondWaitLoop() {}
};

class CondWaitDB {
protected:
	class WriteMeta {
	public:
		WriteMeta(address_t a):addr(a) {}
		~WriteMeta() {}
		address_t addr;
	};
	//use lock to differentiate writes 
	class LockWritesMeta {
	public:
		typedef std::deque<WriteMeta *> WriteMetaDeque;
		LockWritesMeta(address_t lk_addr):lock_addr(lk_addr) {}
		~LockWritesMeta() {
			for(WriteMetaDeque::iterator iter=write_meta_deq.begin();
				iter!=write_meta_deq.end();iter++) {
				delete *iter;
			}
		}
		void AddWriteMeta(WriteMeta *wr_meta) {
			if(write_meta_deq.size()>WRITE_META_LEN) {
				WriteMeta *tmp=write_meta_deq.front();
				write_meta_deq.pop_front();
				delete tmp;
			}
			write_meta_deq.push_back(wr_meta);
		}
		address_t lock_addr;
		WriteMetaDeque write_meta_deq;
	};
	//use vector clock to differentiate a lock signal meta
	class LockSignalMeta {
	public:
		typedef std::deque<LockWritesMeta *> LockWritesMetaDeque;
		LockSignalMeta(VectorClock &vector_clock):vc(vector_clock),
			expired(false),actived(false) {}
		~LockSignalMeta() {
			for(LockWritesMetaDeque::iterator iter=lk_wrs_metas.begin();
				iter!=lk_wrs_metas.end();iter++) {
				delete *iter;
			}
		}
		void AddLockWritesMeta(LockWritesMeta *lk_wrs_meta) {
			lk_wrs_metas.push_back(lk_wrs_meta);
		}
		LockWritesMeta *GetLastestLockWritesMeta() {
			return lk_wrs_metas.back();
		}

		LockWritesMetaDeque lk_wrs_metas;
		VectorClock vc;
		bool expired;
		bool actived; //encounter the signal to active
	};
	class CondWaitMeta {
	public:
		CondWaitMeta(LockSignalMeta *meta,Inst *inst):lsmeta(meta),
			rdinst(inst) {}
		~CondWaitMeta() {}
		LockSignalMeta *lsmeta;
		Inst *rdinst;
	};
public:
	typedef std::tr1::unordered_map<int,Loop *> CondWaitLoopTable;
	typedef std::tr1::unordered_map<std::string,CondWaitLoopTable *> 
		CondWaitLoopMap;
	typedef std::tr1::unordered_set<std::string> 
		ExitingCondLineSet;
	typedef std::deque<LockSignalMeta *> LockSignalMetaDeque;
	typedef std::map<thread_t,LockSignalMetaDeque *> ThreadLSMetasMap;
	typedef std::map<thread_t,CondWaitMeta *> ThreadCWMetaMap;
	typedef std::map<thread_t,address_t> ThreadLockMap;
	CondWaitDB();
	virtual ~CondWaitDB();
	void LoadCondWait(const char *file_name);

	void AddLockSignalWriteMeta(thread_t thd_id, VectorClock &vc,
		address_t addr,address_t lk_addr);
	LockSignalMeta *CorrespondingLockSignalMeta(VectorClock &vc,
		address_t addr,address_t lk_addr);
	
	void AddCondWaitMeta(thread_t thd_id,LockSignalMeta *lsmeta,
		Inst *rdinst);
	void RemoveCondWaitMeta(thread_t thd_id);	

	bool CondWaitRead(Inst *inst,RaceEventType type);
	bool CondWaitRead(std::string &file_name,int line,
		RaceEventType type);

	void AddLastestLock(thread_t curr_thd_id,address_t lk_addr) {
		thd_lock_map_[curr_thd_id]=lk_addr;
	}
	address_t GetLastestLock(thread_t curr_thd_id) {
		if(thd_lock_map_.find(curr_thd_id)==thd_lock_map_.end())
			return 0;
		return thd_lock_map_[curr_thd_id];
	}

	void RemoveUnactivedLockWritesMeta(thread_t curr_thd_id,
		address_t lk_addr);
	void ActiveLockSignalMeta(thread_t curr_thd_id);

	void ProcessSignalCondWaitSync(thread_t curr_thd_id,Inst *curr_inst,
		VectorClock &curr_vc);
	bool ProcessCondWaitRead(thread_t curr_thd_id,Inst *curr_inst,
		VectorClock &curr_vc,address_t addr,std::string &file_name,
		int line);
protected:
	CondWaitLoopMap cwloop_map_;
	ExitingCondLineSet exiting_cond_line_set_;
	//signal thread's metas mapping
	ThreadLSMetasMap thd_lsmetas_map_;
	//cond_wait thread's meta mapping
	ThreadCWMetaMap thd_cwmeta_map_;
	//thread lastest lock mapping
	ThreadLockMap thd_lock_map_;
private:
	DISALLOW_COPY_CONSTRUCTORS(CondWaitDB);
};

}//namespace race

#endif //__RACE_COND_WAIT_H
