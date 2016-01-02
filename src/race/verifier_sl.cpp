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
	Verifier::Register();
	knob_->RegisterBool("race_verify_sl","whether enable the race verify_sl",
		"0");
	// knob_->RegisterInt("unit_size_","the mornitoring granularity in bytes",
	// 	"4");
	// knob_->RegisterInt("ss_deq_len","max length of the snapshot deque","10");
	// knob_->RegisterStr("exiting_cond_lines","spin reads in each loop",
	// 	"0");
	// knob_->RegisterStr("cond_wait_lines","condition variable relevant signal"
	// 	" and wait region","0");
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
	if(thd_rdlkcnt_map_.find(curr_thd_id)==thd_rdlkcnt_map_.end())
		thd_rdlkcnt_map_[curr_thd_id]=1;
	else
		++thd_rdlkcnt_map_[curr_thd_id];	
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
	if(rwlock_meta->GetWrlockOwner()==curr_thd_id) {
		DEBUG_ASSERT(thd_lkcnt_map_.find(curr_thd_id)!=thd_lkcnt_map_.end()
			&& thd_lkcnt_map_[curr_thd_id]>0);
		--thd_lkcnt_map_[curr_thd_id];	
	} else {
		DEBUG_ASSERT(thd_rdlkcnt_map_.find(curr_thd_id)!=thd_rdlkcnt_map_.end()
			&& thd_rdlkcnt_map_[curr_thd_id]>0);
		--thd_rdlkcnt_map_[curr_thd_id];
	}
}

void VerifierSl::AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
	timestamp_t curr_thd_clk,RaceEventType type,Inst *inst,PStmt *s) 
{
	//calculate the lockcount
	if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
		thd_lkcnt_map_[curr_thd_id]=0;
	if(thd_rdlkcnt_map_.find(curr_thd_id)==thd_rdlkcnt_map_.end())
		thd_rdlkcnt_map_[curr_thd_id]=0;
	SlMetaSnapshot *slmeta_ss=new SlMetaSnapshot(curr_thd_clk,type,
		inst,s);
	slmeta_ss->lock_count=thd_lkcnt_map_[curr_thd_id];
	slmeta_ss->rdlock_count=thd_rdlkcnt_map_[curr_thd_id];
	meta->AddMetaSnapshot(curr_thd_id,slmeta_ss);
}

RaceType VerifierSl::HistoryRace(MetaSnapshot *meta_ss,thread_t thd_id,
	thread_t curr_thd_id,RaceEventType curr_type) 
{
	SlMetaSnapshot *slmeta_ss=dynamic_cast<SlMetaSnapshot *>(meta_ss);
	timestamp_t thd_clk=thd_vc_map_[curr_thd_id]->GetClock(thd_id);

	if(thd_lkcnt_map_.find(curr_thd_id)==thd_lkcnt_map_.end())
		thd_lkcnt_map_[curr_thd_id]=0;
	if(thd_rdlkcnt_map_.find(curr_thd_id)==thd_rdlkcnt_map_.end())
		thd_rdlkcnt_map_[curr_thd_id]=0;
	
	if(curr_type==RACE_EVENT_WRITE && slmeta_ss->thd_clk>thd_clk) {
		if(slmeta_ss->type==RACE_EVENT_WRITE && 
			(thd_lkcnt_map_[curr_thd_id]==0 || slmeta_ss->lock_count==0)) {
			return WRITETOWRITE;
		}
		if(slmeta_ss->type==RACE_EVENT_READ &&
			(thd_lkcnt_map_[curr_thd_id]==0 || 
				(slmeta_ss->lock_count==0 && slmeta_ss->rdlock_count==0))) {
			return READTOWRITE;
		}
	}
	else if(curr_type==RACE_EVENT_READ && slmeta_ss->thd_clk>thd_clk) {
		if(slmeta_ss->type==RACE_EVENT_WRITE &&
			((thd_rdlkcnt_map_[curr_thd_id]==0 && 
				thd_lkcnt_map_[curr_thd_id]==0) || slmeta_ss->lock_count==0)) {
			return WRITETOREAD;
		}
	}
	return NONE;
}

} //namespace race