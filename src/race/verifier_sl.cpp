#include "race/verifier_sl.h"
#include "core/log.h"

namespace race
{
VerifierSl::VerifierSl()
{}

VerifierSl::~VerifierSl()
{}

size_t VerifierSl::ss_vec_len=0;

bool VerifierSl::Enabled()
{
	return knob_->ValueBool("race_verify_sl");
}

void VerifierSl::Register()
{
	knob_->RegisterBool("race_verify_sl","whether enable the race verify_sl",
		"0");
	knob_->RegisterInt("unit_size_","the mornitoring granularity in bytes",
		"4");
	knob_->RegisterInt("ss_vec_len","max length of the snapshot vector","10");
}

void VerifierSl::Setup(Mutex *internal_lock,Mutex *verify_lock,PRaceDB *prace_db)
{
	Verifier::Setup(internal_lock,verify_lock,prace_db);
	ss_vec_len=knob_->ValueInt("ss_vec_len");
}

Verifier::Meta *VerifierSl::GetMeta(address_t addr)
{
	Meta::Table::iterator iter=meta_table_.find(addr);
	if(iter==meta_table_.end()) {
		Meta *meta=new SlMeta(addr);
		meta_table_[addr]=meta;
		return meta;
	}
	return iter->second;
}

inline void VerifierSl::ProcessPostMutexLock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//do not change the vector clock
	mutex_meta->SetOwner(curr_thd_id);
	UnblockThread(curr_thd_id);
	//calculate current thread's lock count
	if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
		thd_lkcnt_map_[curr_thd_id]=1;
	else
		++thd_lkcnt_map_[curr_thd_id];
}

inline void VerifierSl::ProcessPreMutexUnlock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//do not change the vector clock
	DEBUG_ASSERT(thd_lkcnt_map_.find(curr_thd_id)!=thd_lkcnt_map_.end()
		&& thd_lkcnt_map_[curr_thd_id]>0);
	--thd_lkcnt_map_[curr_thd_id];
}

inline void VerifierSl::ProcessPostRwlockRdlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	//do not change the vector clock
	UnblockThread(curr_thd_id);
	rwlock_meta->AddRdlockOwner(curr_thd_id);
	//do the same work like mutex_lock
	if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
		thd_lkcnt_map_[curr_thd_id]=1;
	else
		++thd_lkcnt_map_[curr_thd_id];	
}

inline void VerifierSl::ProcessPostRwlockWrlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	//do not change the vector clock
	UnblockThread(curr_thd_id);
	rwlock_meta->SetWrlockOwner(curr_thd_id);
	//do the same work like mutex_lock
	if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
		thd_lkcnt_map_[curr_thd_id]=1;
	else
		++thd_lkcnt_map_[curr_thd_id];	
}

inline void VerifierSl::ProcessPreRwlockUnlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	//do not change the vector clock
	//do the same work like mutex_unlock
	DEBUG_ASSERT(thd_lkcnt_map_.find(curr_thd_id)!=thd_lkcnt_map_.end()
		&& thd_lkcnt_map_[curr_thd_id]>0);
	--thd_lkcnt_map_[curr_thd_id];
}

} //namespace race