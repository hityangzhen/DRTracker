#ifndef __VERIFIER_SL_H
#define __VERIFIER_SL_H

//Use the simple_lock method to decrease false positive

#include "race/verifier.h"
#include "core/log.h"

namespace race 
{
class VerifierSl:public Verifier {
public:
	typedef std::tr1::unordered_map<thread_t,uint8> ThreadLockCountMap;
	VerifierSl();
	~VerifierSl();
	void Register();
	bool Enabled();
	void Setup(Mutex *internal_lock,Mutex *verify_lock,PRaceDB *prace_db);

private:
	class SlMetaSnapshot:public MetaSnapshot {
	public:
		SlMetaSnapshot(timestamp_t clk,RaceEventType t,Inst *i,uint8 lkcnt):
			MetaSnapshot(clk,t,i),lock_count(lkcnt) {}
		~SlMetaSnapshot() {}
		uint8 lock_count;
	};
	class SlMeta:public Meta {
	public:
		explicit SlMeta(address_t a):Meta(a),index(0) {}
		~SlMeta() {}
		int index;
		Verifier::MetaSnapshot *GetLastMetaSnapshot(thread_t thd_id) {
			if(meta_ss_map.find(thd_id)==meta_ss_map.end())
				return NULL;
			return meta_ss_map[thd_id]->back();
		}
		//overrite
		void AddMetaSnapshot(thread_t thd_id,MetaSnapshot *meta_ss){
			if(meta_ss_map.find(thd_id)==meta_ss_map.end())
				meta_ss_map[thd_id]=
					new MetaSnapshotVector(VerifierSl::ss_vec_len,NULL);
			//too many snapshots
			if(meta_ss_map[thd_id]->size()==VerifierSl::ss_vec_len)
				index=0;
			(*meta_ss_map[thd_id])[index++]=meta_ss;
		}
		MetaSnapshot* LastestMetaSnapshot(thread_t thd_id) {
			int tmp=(index+VerifierSl::ss_vec_len-1)%VerifierSl::ss_vec_len;
			return (*meta_ss_map[thd_id])[tmp];
		}
	};

	ThreadLockCountMap thd_lkcnt_map_;
	static size_t ss_vec_len;
private:
	//overrite 
	Meta* GetMeta(address_t addr);

	void ProcessPostMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
	void ProcessPreMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);

	void ProcessPostRwlockRdlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPostRwlockWrlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPreRwlockUnlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);

	bool HistoryRaceCondition(MetaSnapshot *meta_ss,thread_t thd_id,
		thread_t curr_thd_id) {
		SlMetaSnapshot *slmeta_ss=dynamic_cast<SlMetaSnapshot *>(meta_ss);
		timestamp_t thd_clk=thd_vc_map_[curr_thd_id]->GetClock(thd_id);
		if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
			thd_lkcnt_map_[curr_thd_id]=0;
		return slmeta_ss->thd_clk>thd_clk && (slmeta_ss->lock_count==0 ||
			thd_lkcnt_map_[curr_thd_id]==0);
	}
	void AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
		timestamp_t curr_thd_clk,RaceEventType type,Inst *inst) {
		if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
			thd_lkcnt_map_[curr_thd_id]=0;
		MetaSnapshot *meta_ss=new SlMetaSnapshot(curr_thd_clk,type,inst,
			thd_lkcnt_map_[curr_thd_id]);
		meta->AddMetaSnapshot(curr_thd_id,meta_ss);
	}
private:
	DISALLOW_COPY_CONSTRUCTORS(VerifierSl);
};

} //namespace race

#endif