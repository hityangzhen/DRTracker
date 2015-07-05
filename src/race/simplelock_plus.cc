#include "race/simplelock_plus.h"
#include "core/log.h"

namespace race {

SimpleLockPlus::SimpleLockPlus():track_racy_inst_(false)
{}

SimpleLockPlus::~SimpleLockPlus()
{}

void SimpleLockPlus::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_simplelock_plus",
		"whether enable the simplelock_plus data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool SimpleLockPlus::Enabled()
{
	return knob_->ValueBool("enable_simplelock_plus");
}

void SimpleLockPlus::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

void SimpleLockPlus::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);	
	//only increment thread's lock count
	if(curr_lc_table_.find(curr_thd_id)==curr_lc_table_.end())
		curr_lc_table_[curr_thd_id]=1;
	else
		++curr_lc_table_[curr_thd_id];
}

void SimpleLockPlus::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	//only decrement thread's lock count
	ThreadLockCountTable::iterator it=curr_lc_table_.find(curr_thd_id);
	DEBUG_ASSERT(it!=curr_lc_table_.end());
	--curr_lc_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lc_table_[curr_thd_id]>=0);
}

void SimpleLockPlus::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void SimpleLockPlus::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void SimpleLockPlus::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	BeforePthreadMutexUnlock(curr_thd_id,curr_thd_clk,inst,addr);
}

Detector::Meta *SimpleLockPlus::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new SlpMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void SimpleLockPlus::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	SlpMeta *slp_meta=dynamic_cast<SlpMeta*>(meta);
	DEBUG_ASSERT(slp_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);

	if(slp_meta->reader_clip_map.find(curr_thd_id)==slp_meta->reader_clip_map.end())
		slp_meta->reader_clip_map[curr_thd_id]=SlpMeta::ClockLockIszeroPair(0,true);
	SlpMeta::ClockLockIszeroPair &reader_clip=slp_meta->reader_clip_map[curr_thd_id];
	//eliminate redudant read access
	if(reader_clip.first==curr_clk && reader_clip.second==true)
		return ;

	bool iszero=false;
	if(curr_lc_table_[curr_thd_id]==0)
		iszero=true;
	//current read access has locks protection
	//eliminate redudant read access
	if(reader_clip.first==curr_clk && !iszero)
		return ;
	else {
		reader_clip.first=curr_clk;
		reader_clip.second=iszero;
	}

	//write-read race
	for(SlpMeta::ThreadClockLockIszeroPairMap::iterator it=
		slp_meta->writer_clip_map.begin();it!=slp_meta->writer_clip_map.end();it++) {

		if(it->first==curr_thd_id)
			continue;

		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);
		SlpMeta::ClockLockIszeroPair &writer_clip=it->second;
		if(writer_clip.first>thd_clk && 
			(writer_clip.second==true || iszero )) {

			//PrintDebugRaceInfo("SIMPLELOCKPLUS",WRITETOREAD,slp_meta,curr_thd_id,inst);

			slp_meta->racy=true;
			Inst *writer_inst=slp_meta->writer_inst_table[thd_id];
			ReportRace(slp_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
												curr_thd_id,inst,RACE_EVENT_READ);
		}
	}

	slp_meta->reader_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		slp_meta->race_inst_set.insert(inst);
}

void SimpleLockPlus::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	SlpMeta *slp_meta=dynamic_cast<SlpMeta*>(meta);
	DEBUG_ASSERT(slp_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);

	if(slp_meta->writer_clip_map.find(curr_thd_id)==slp_meta->writer_clip_map.end())
		slp_meta->writer_clip_map[curr_thd_id]=SlpMeta::ClockLockIszeroPair(0,true);
	SlpMeta::ClockLockIszeroPair &writer_clip=slp_meta->writer_clip_map[curr_thd_id];
	//eliminate redudant write access
	if(writer_clip.first==curr_clk && writer_clip.second==true)
		return ;

	bool iszero=false;
	if(curr_lc_table_[curr_thd_id]==0)
		iszero=true;
	//current write access has locks protection and prior write access also has
	//locks protection , so will not bring additional changes
	if(writer_clip.first==curr_clk && !iszero)
		return ;
	else {
		writer_clip.first=curr_clk;
		writer_clip.second=iszero;
	}

	//write-write race
	for(SlpMeta::ThreadClockLockIszeroPairMap::iterator it=
		slp_meta->writer_clip_map.begin();it!=slp_meta->writer_clip_map.end();it++) {
		if(it->first==curr_thd_id)
			continue ;

		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);

		SlpMeta::ClockLockIszeroPair &writer_clip2=it->second;
		if(writer_clip2.first>thd_clk && 
			(writer_clip2.second==true || iszero)) {

			//PrintDebugRaceInfo("SIMPLELOCKPLUS",WRITETOWRITE,slp_meta,curr_thd_id,inst);

			slp_meta->racy=true;
			Inst *writer_inst=slp_meta->writer_inst_table[thd_id];
			ReportRace(slp_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
													curr_thd_id,inst,RACE_EVENT_WRITE);
		}
	}

	//read-write race
	for(SlpMeta::ThreadClockLockIszeroPairMap::iterator it=
		slp_meta->reader_clip_map.begin();it!=slp_meta->reader_clip_map.end();it++) {
		if(it->first==curr_thd_id)
			continue ;

		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);
		SlpMeta::ClockLockIszeroPair &reader_clip=it->second;
		//if thd_clk is 0 ,indicates that current thread is unknown to corresponding
		//thread clock
		if(reader_clip.first>thd_clk &&
			(reader_clip.second==true || iszero)) {

			//PrintDebugRaceInfo("SIMPLELOCKPLUS",READTOWRITE,slp_meta,curr_thd_id,inst);

			slp_meta->racy=true;
			Inst *reader_inst=slp_meta->reader_inst_table[thd_id];
			ReportRace(slp_meta,thd_id,reader_inst,RACE_EVENT_READ,
													curr_thd_id,inst,RACE_EVENT_WRITE);	
		}
	}

	slp_meta->writer_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		slp_meta->race_inst_set.insert(inst);
}

void SimpleLockPlus::ProcessFree(Meta *meta)
{
	SlpMeta *slp_meta=dynamic_cast<SlpMeta *>(meta);
	DEBUG_ASSERT(slp_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && slp_meta->racy) {
		for(SlpMeta::InstSet::iterator it=slp_meta->race_inst_set.begin();
			it!=slp_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	delete slp_meta;
}

}//namespace race