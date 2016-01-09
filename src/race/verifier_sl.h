#ifndef __VERIFIER_SL_H
#define __VERIFIER_SL_H

//Use the simple_lock dynamic data race detection engine to
//improve the precision.

#include "race/verifier.h"

namespace race 
{
class VerifierSl:public Verifier {
public:
	typedef std::tr1::unordered_map<thread_t,uint8> ThreadLockCountMap;
	VerifierSl();
	~VerifierSl();
	void Register();
	bool Enabled();
private:
	class SlMetaSnapshot:public MetaSnapshot {
	public:
		SlMetaSnapshot(timestamp_t clk,RaceEventType t,Inst *i,PStmt *s):
			MetaSnapshot(clk,t,i,s) {}
		~SlMetaSnapshot() {}
		uint8 lock_count;
		uint8 rdlock_count;
	};
	class SlMeta:public Meta {
	public:
		explicit SlMeta(address_t addr):Meta(addr) {}
		~SlMeta() {}
	};

	ThreadLockCountMap thd_lkcnt_map_;
	ThreadLockCountMap thd_rdlkcnt_map_;
	static size_t ss_vec_len;
private:
	//overrite 
	Meta* GetMeta(address_t addr);

	void ProcessPostMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
	void ProcessPreMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);

	void ProcessPostRwlockRdlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPostRwlockWrlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPreRwlockUnlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);

	void AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
		timestamp_t curr_thd_clk,RaceEventType type,Inst *inst,PStmt *s);
	RaceType HistoryRace(MetaSnapshot *meta_ss,thread_t thd_id,
		thread_t curr_thd_id,RaceEventType curr_type);
private:
	DISALLOW_COPY_CONSTRUCTORS(VerifierSl);
};

} //namespace race

#endif