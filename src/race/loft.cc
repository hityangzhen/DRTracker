#include "race/loft.h"
#include "core/log.h"


namespace race {

void Loft::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_loft",
		"whether enable the loft data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool Loft::Enabled()
{
	return knob_->ValueBool("enable_loft");
}

void Loft::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}

//Loft LRG rule
void Loft::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock locker(internal_lock_);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  	MutexMeta *meta = GetMutexMeta(addr);
  	DEBUG_ASSERT(meta);
  	ProcessUnlock(curr_thd_id, meta);
}

void Loft::ProcessLock(thread_t curr_thd_id,MutexMeta *meta)
{
	LoftMutexMeta *loftmutex_meta=dynamic_cast<LoftMutexMeta *>(meta);
	//LRDB rule:lastThread(m)==t
	//current thread's vc greater or equal than mutex's vc
	//do not need comparison and join to thread's vc
	if(curr_thd_id==loftmutex_meta->lastrld_thd_id)
		return ;
	FastTrack::ProcessLock(curr_thd_id,loftmutex_meta);
}

Detector::MutexMeta *Loft::GetMutexMeta(address_t iaddr)
{
	MutexMeta::Table::iterator it=mutex_meta_table_.find(iaddr);
	if(it==mutex_meta_table_.end()) {
		MutexMeta *meta=new LoftMutexMeta;
		mutex_meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void Loft::ProcessUnlock(thread_t curr_thd_id,MutexMeta *meta)
{
	LoftMutexMeta *loftmutex_meta=dynamic_cast<LoftMutexMeta *>(meta);
	//LRG rule:(lastThread(m)==t and lastLock(t)==m) or 
	//		   (lastThread(m)!=t and lastLock(t)==m)
	//current thread's vc smaller or equal than mutex's vc
	//do not need comparison and assignment to mutex's vc
	if(thread_lastrldlock_map_.find(curr_thd_id)!=
		thread_lastrldlock_map_.end()) {
		//very important
		if(thread_lastrldlock_map_[curr_thd_id]==loftmutex_meta
			&& (loftmutex_meta->vc.GetClock(curr_thd_id) >=
			curr_vc_map_[curr_thd_id]->GetClock(curr_thd_id)) ) {
			//update the last released thread of mutex
			loftmutex_meta->lastrld_thd_id=curr_thd_id;
			//current thread vc add
			curr_vc_map_[curr_thd_id]->Increment(curr_thd_id);
			return ;
		}
	}
	loftmutex_meta->lastrld_thd_id=curr_thd_id;
	thread_lastrldlock_map_[curr_thd_id]=
		static_cast<MutexMeta *>(loftmutex_meta);
	FastTrack::ProcessUnlock(curr_thd_id,loftmutex_meta);	
}

}// namespace race