#include "race/verifier_ml.h"
#include "core/log.h"

namespace race
{
VerifierMl::VerifierMl() 
{}

VerifierMl::~VerifierMl()
{
	//clear the lockset
	for(ThreadLockSetMap::iterator iter=thd_ls_map_.begin();
		iter!=thd_ls_map_.end();iter++)
		delete iter->second;
	for(ThreadLockSetMap::iterator iter=thd_rdls_map_.begin();
		iter!=thd_rdls_map_.end();iter++)
		delete iter->second;
}

bool VerifierMl::Enabled()
{
	return knob_->ValueBool("race_verify_ml");
}

void VerifierMl::Register()
{
	Verifier::Register();
	knob_->RegisterBool("race_verify_ml","whether enable the race_verify_ml",
		"0");
	// knob_->RegisterInt("unit_size_","the mornitoring granularity in bytes",
	// 	"4");
	// knob_->RegisterInt("ss_deq_len","max length of the snapshot deque","10");
	// knob_->RegisterStr("exiting_cond_lines","spin reads in each loop",
	// 	"0");
	// knob_->RegisterStr("cond_wait_lines","condition variable relevant signal"
	// 	" and wait region","0");
}

Verifier::Meta* VerifierMl::GetMeta(address_t addr)
{
	Meta::Table::iterator iter=meta_table_.find(addr);
	if(iter==meta_table_.end()) {
		Meta *meta=new MlMeta(addr);
		meta_table_[addr]=meta;
		return meta;
	}
	return iter->second;
}

inline void VerifierMl::ProcessPostMutexLock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	mutex_meta->SetOwner(curr_thd_id);
	UnblockThread(curr_thd_id);

	//calculate the lockset
	if(thd_ls_map_.find(curr_thd_id)==thd_ls_map_.end())
		thd_ls_map_[curr_thd_id]=new LockSet;
	thd_ls_map_[curr_thd_id]->Add(mutex_meta->addr);
}

inline void VerifierMl::ProcessPreMutexUnlock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//calculate the lockset
	LockSet *curr_ls=thd_ls_map_[curr_thd_id];
	DEBUG_ASSERT(curr_ls && curr_ls->Exist(mutex_meta->addr));
	curr_ls->Remove(mutex_meta->addr);
}

inline void VerifierMl::ProcessPostRwlockRdlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	UnblockThread(curr_thd_id);
	rwlock_meta->AddRdlockOwner(curr_thd_id);
	//calculate the lockset
	if(thd_rdls_map_.find(curr_thd_id)==thd_rdls_map_.end())
		thd_rdls_map_[curr_thd_id]=new LockSet;
	thd_rdls_map_[curr_thd_id]->Add(rwlock_meta->addr);
}

inline void VerifierMl::ProcessPostRwlockWrlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	UnblockThread(curr_thd_id);
	rwlock_meta->SetWrlockOwner(curr_thd_id);
	//calculate the lockset
	if(thd_ls_map_.find(curr_thd_id)==thd_ls_map_.end())
		thd_ls_map_[curr_thd_id]=new LockSet;
	thd_ls_map_[curr_thd_id]->Add(rwlock_meta->addr);
}

inline void VerifierMl::ProcessPreRwlockUnlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	if(thd_ls_map_.find(curr_thd_id)!=thd_ls_map_.end()) {
		DEBUG_ASSERT(thd_ls_map_[curr_thd_id]);
		thd_ls_map_[curr_thd_id]->Remove(rwlock_meta->addr);
	}
	if(thd_rdls_map_.find(curr_thd_id)!=thd_rdls_map_.end()) {
		DEBUG_ASSERT(thd_rdls_map_[curr_thd_id]);
		thd_rdls_map_[curr_thd_id]->Remove(rwlock_meta->addr);
	}
}

void VerifierMl::AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
	timestamp_t curr_thd_clk,RaceEventType type,Inst *inst,PStmt *s)
{
	//calculate the lockset
	if(thd_rdls_map_.find(curr_thd_id)==thd_rdls_map_.end())
		thd_rdls_map_[curr_thd_id]=new LockSet;
	if(thd_ls_map_.find(curr_thd_id)==thd_ls_map_.end())
		thd_ls_map_[curr_thd_id]=new LockSet;
	MlMetaSnapshot *mlmeta_ss=new MlMetaSnapshot(curr_thd_clk,type,
		inst,s);
	mlmeta_ss->rd_ls=*thd_rdls_map_[curr_thd_id];
	mlmeta_ss->wr_ls=*thd_ls_map_[curr_thd_id];
	meta->AddMetaSnapshot(curr_thd_id,mlmeta_ss);
}

RaceType VerifierMl::HistoryRace(MetaSnapshot *meta_ss,thread_t thd_id,
	thread_t curr_thd_id,RaceEventType curr_type) 
{
	MlMetaSnapshot *mlmeta_ss=dynamic_cast<MlMetaSnapshot*>(meta_ss);
	timestamp_t thd_clk=thd_vc_map_[curr_thd_id]->GetClock(thd_id);

	if(thd_rdls_map_.find(curr_thd_id)==thd_rdls_map_.end())
		thd_rdls_map_[curr_thd_id]=new LockSet;
	if(thd_ls_map_.find(curr_thd_id)==thd_ls_map_.end())
		thd_ls_map_[curr_thd_id]=new LockSet;

	LockSet *curr_rdls=thd_rdls_map_[curr_thd_id];
	LockSet *curr_ls=thd_ls_map_[curr_thd_id];

// INFO_FMT_PRINT("===========meta_ss clk:[%ld],meta_ss type:[%d],thd_clk:[%ld]=============\n",
// 	mlmeta_ss->thd_clk,mlmeta_ss->type,thd_clk);

	if(curr_type==RACE_EVENT_WRITE && mlmeta_ss->thd_clk>thd_clk) {
		if(mlmeta_ss->type==RACE_EVENT_WRITE && 
			mlmeta_ss->wr_ls.Disjoint(curr_ls)) {
			return WRITETOWRITE;
		}
		if(mlmeta_ss->type==RACE_EVENT_READ &&
			mlmeta_ss->wr_ls.Disjoint(curr_ls) && 
			mlmeta_ss->rd_ls.Disjoint(curr_ls) &&
			mlmeta_ss->wr_ls.Disjoint(curr_rdls)) {
			return READTOWRITE;
		}
	}
	else if(curr_type==RACE_EVENT_READ && mlmeta_ss->thd_clk>thd_clk) {
		if(mlmeta_ss->type==RACE_EVENT_WRITE && 
			mlmeta_ss->wr_ls.Disjoint(curr_ls) && 
			mlmeta_ss->wr_ls.Disjoint(curr_rdls) &&
			mlmeta_ss->rd_ls.Disjoint(curr_ls)) {
			return WRITETOREAD;
		} 
	}

	return NONE;
}

} //namspace race