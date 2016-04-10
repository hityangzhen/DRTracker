#ifndef __VERIFIER_ML_H
#define __VERIFIER_ML_H

//Use the multilock-hb dynamic data race detection engine
//to improve the precision.

#include "race/verifier.h"
#include "core/lock_set.h"

namespace race
{
class VerifierMl:public Verifier{
public:
	typedef std::tr1::unordered_map<thread_t,LockSet *> ThreadLockSetMap;
	VerifierMl();
	virtual ~VerifierMl();
	virtual void Register();
	virtual bool Enabled();

protected:
	class MlMetaSnapshot:public MetaSnapshot {
	public:
		MlMetaSnapshot(timestamp_t clk,RaceEventType t,Inst *i,PStmt *s):
			MetaSnapshot(clk,t,i,s) {}
		~MlMetaSnapshot() {}
		LockSet wr_ls;
		LockSet rd_ls;
	};

	class MlMeta:public Meta {
	public:
		explicit MlMeta(address_t addr):Meta(addr) {}
		~MlMeta() {}
	};
	//for mx_lock or wr_lock
	ThreadLockSetMap thd_ls_map_;
	//for rd_lock
	ThreadLockSetMap thd_rdls_map_;
protected:
	//overrite
	Meta *GetMeta(address_t addr);

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
	DISALLOW_COPY_CONSTRUCTORS(VerifierMl);
};

} //namespace race

#endif