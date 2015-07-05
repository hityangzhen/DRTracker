#include "race/race_track.h"
#include "core/log.h"

namespace race {

RaceTrack::RaceTrack():track_racy_inst_(false)
{}

RaceTrack::~RaceTrack()
{
	for(LockSetTable::iterator it=reader_lock_set_table_.begin();
		it!=reader_lock_set_table_.end();it++) 
		delete it->second;
	for(LockSetTable::iterator it=writer_lock_set_table_.begin();
		it!=writer_lock_set_table_.end();it++) 
		delete it->second;
}

void RaceTrack::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_race_track",
		"whether enable the race_track data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool RaceTrack::Enabled()
{
	return knob_->ValueBool("enable_race_track");
}

void RaceTrack::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

Detector::Meta * RaceTrack::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new RtMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void RaceTrack::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
}

void RaceTrack::AfterPthreadMutexTryLock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	if(ret_val!=0)
		return ;
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void RaceTrack::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//
	curr_writer_lock_set->Remove(addr);
	curr_reader_lock_set->Remove(addr);
}

void RaceTrack::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	//construct a writer lockset struct without locks
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_reader_lock_set && curr_writer_lock_set);
	//reader lock can only protect read access
	if(lock_reference_map_.find(addr)!=lock_reference_map_.end())
		lock_reference_map_[addr]+=1;
	else
		lock_reference_map_[addr]=1;
	curr_reader_lock_set->Add(addr);
}

void RaceTrack::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	lock_reference_map_[addr]=1;
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
}

void RaceTrack::AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void RaceTrack::AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t addr,int ret_val)
{
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockWrlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void RaceTrack::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{

	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	DEBUG_ASSERT(lock_reference_map_[addr]>0);

	if(lock_reference_map_[addr]==1) {
		curr_writer_lock_set->Remove(addr);
		curr_reader_lock_set->Remove(addr);	
	}
	lock_reference_map_[addr]-=1;
}

void RaceTrack::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process read\n");
	RtMeta *rt_meta=dynamic_cast<RtMeta*>(meta);
	DEBUG_ASSERT(rt_meta);
	if(rt_meta->racy)
		return;
	UpdateMemoryState(curr_thd_id,RACE_EVENT_READ,rt_meta);

	//racy
	if(rt_meta->state==RtMeta::MEM_STATE_RACY) {
		//write-read race
		{
			rt_meta->racy=true;
			//report all potiential races
			//doesn't know which is conflict with current thread's read
			RtMeta::ThreadSet::iterator it=rt_meta->thread_set.begin();
			for(;it!=rt_meta->thread_set.end(); it++) {
				if(it->first==curr_thd_id || rt_meta->writer_inst_table.find(it->first)
					==rt_meta->writer_inst_table.end())
					continue;

				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				DEBUG_FMT_PRINT_SAFE("WAR race detected [T%lx]\n",curr_thd_id);
				DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",rt_meta->addr);
				DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n",inst->ToString().c_str());
				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				Inst *writer_inst=rt_meta->writer_inst_table[it->first];
				ReportRace(rt_meta,it->first,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);
			}
		}
	}
	rt_meta->reader_inst_table[curr_thd_id]=inst;
	if(track_racy_inst_)
		rt_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process read end\n");
}

void RaceTrack::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	RtMeta *rt_meta=dynamic_cast<RtMeta*>(meta);
	DEBUG_ASSERT(rt_meta);
	if(rt_meta->racy)
		return;

	UpdateMemoryState(curr_thd_id,RACE_EVENT_WRITE,rt_meta);
	INFO_FMT_PRINT("rt_meta->writer_lock_set:%s\n",
		rt_meta->writer_lock_set.ToString().c_str());	
	INFO_FMT_PRINT("rt_meta->reader_lock_set:%s\n",
		rt_meta->reader_lock_set.ToString().c_str());
	INFO_FMT_PRINT("state:%d\n",rt_meta->state);
	INFO_FMT_PRINT("thread_set size:%ld\n",rt_meta->thread_set.size());
	
	//racy
	if(rt_meta->state==RtMeta::MEM_STATE_RACY) {
		//INFO_PRINT("racy\n");
		//write-write race
		rt_meta->racy=true;
		{
			RtMeta::ThreadSet::iterator it=rt_meta->thread_set.begin();
			for(;it!=rt_meta->thread_set.end(); it++) {
				if(it->first==curr_thd_id || rt_meta->writer_inst_table.find(it->first)
					==rt_meta->writer_inst_table.end())
					continue;

				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				DEBUG_FMT_PRINT_SAFE("WAW race detected [T%lx]\n",curr_thd_id);
				DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",rt_meta->addr);
				DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n",inst->ToString().c_str());
				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				Inst *writer_inst=rt_meta->writer_inst_table[it->first];
				ReportRace(rt_meta,it->first,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
		//read-write race
		{
			RtMeta::ThreadSet::iterator it=rt_meta->thread_set.begin();
			for(;it!=rt_meta->thread_set.end(); it++) {
				if(it->first==curr_thd_id || rt_meta->reader_inst_table.find(it->first)
					==rt_meta->reader_inst_table.end())
					continue;

				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				DEBUG_FMT_PRINT_SAFE("RAW race detected [T%lx]\n",curr_thd_id);
				DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",rt_meta->addr);
				DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n",inst->ToString().c_str());
				DEBUG_FMT_PRINT_SAFE("%sRACETRACK DETECTOR%s\n",SEPARATOR,SEPARATOR);
				Inst *reader_inst=rt_meta->reader_inst_table[it->first];
				ReportRace(rt_meta,it->first,reader_inst,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_READ);
			}
		}

	}
	rt_meta->writer_inst_table[curr_thd_id]=inst;
	if(track_racy_inst_)
		rt_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process write end\n");
}

void RaceTrack::UpdateMemoryState(thread_t curr_thd_id,RaceEventType race_event_type,
	RtMeta *rt_meta)
{
	// INFO_FMT_PRINT("writer_lock_set:%s\n",rt_meta->writer_lock_set.ToString().c_str());	
	// INFO_FMT_PRINT("thread_set size:%ld\n",rt_meta->thread_set.size());	
	// for(RtMeta::ThreadSet::iterator it=rt_meta->thread_set.begin();
	// 	it!=rt_meta->thread_set.end();it++)
	// 	INFO_FMT_PRINT("[%lx:%ld] ",it->first,it->second);

	// INFO_FMT_PRINT("\ncurr_vc:%s\n",curr_vc_map_[curr_thd_id]->ToString().c_str());
	// INFO_FMT_PRINT("state:%d\n",rt_meta->state);
	//constructed initially
	if(rt_meta->state==RtMeta::MEM_STATE_VIRGIN) {
		rt_meta->state=RtMeta::MEM_STATE_EXCLUSIVE1;
		rt_meta->owner_thd_id=curr_thd_id;
		//initialize the thread set and lockset
		MergeThreadSet(curr_thd_id,rt_meta);
		InitializeLockSet(curr_thd_id,race_event_type,rt_meta);	
		return ;
	}
	// first transition
	// for some server programs (object-passing style)
	// if(rt_meta->state==RtMeta::MEM_STATE_EXCLUSIVE0 && 
	// 	rt_meta->owner_thd_id!=curr_thd_id) {
	// 	rt_meta->state=RtMeta::MEM_STATE_EXCLUSIVE1;
	// 	//initialize the thread set and lockset
	// 	MergeThreadSet(curr_thd_id,rt_meta);
	// 	InitializeLockSet(curr_thd_id,race_event_type,rt_meta);
	// 	return ;
	// }
	if(rt_meta->state==RtMeta::MEM_STATE_EXCLUSIVE1) {
		//track the thread set and lockset
		MergeThreadSet(curr_thd_id,rt_meta);
		MergeLockSet(curr_thd_id,race_event_type,rt_meta);
		//concurrent access
		if(rt_meta->thread_set.size()>1) {
			if(race_event_type==RACE_EVENT_READ) {
				rt_meta->state=RtMeta::MEM_STATE_READ_SHARED;
				//discard the thread set
				rt_meta->thread_set.clear();
				return ;
			}
			else if(race_event_type==RACE_EVENT_WRITE) {
				rt_meta->state=RtMeta::MEM_STATE_SHARED_MODIFIED1;
				//discard the thread set
				rt_meta->thread_set.clear();
			}
		}
		else
			return ;
	}
	if(rt_meta->state==RtMeta::MEM_STATE_READ_SHARED) {
		MergeLockSet(curr_thd_id,race_event_type,rt_meta);
		if(race_event_type==RACE_EVENT_WRITE && 
			rt_meta->writer_lock_set.Empty()) {
			rt_meta->state=RtMeta::MEM_STATE_EXCLUSIVE2;
			//initialize the thread set and lock set
			//EXCLUSIVE2 will initialize the thread set,here emit
			InitializeLockSet(curr_thd_id,race_event_type,rt_meta);
		}	
		else if(race_event_type==RACE_EVENT_WRITE && 
			!rt_meta->writer_lock_set.Empty()) {
			rt_meta->state=RtMeta::MEM_STATE_SHARED_MODIFIED1;
			return ;
		}
		else
			return ;
	}
	if(rt_meta->state==RtMeta::MEM_STATE_SHARED_MODIFIED1) {
		MergeLockSet(curr_thd_id,race_event_type,rt_meta);
		if(rt_meta->writer_lock_set.Empty()) {
			rt_meta->state=RtMeta::MEM_STATE_EXCLUSIVE2;
			//initialize the thread set and lock set
			//EXCLUSIVE2 will initialize the thread set,here emit
			InitializeLockSet(curr_thd_id,race_event_type,rt_meta);
		}
		else
			return ;
	}
	//defer the thread set checking
	if(rt_meta->state==RtMeta::MEM_STATE_EXCLUSIVE2) {
		MergeThreadSet(curr_thd_id,rt_meta);
		MergeLockSet(curr_thd_id,race_event_type,rt_meta);
		if(rt_meta->thread_set.size()>1)
			rt_meta->state=RtMeta::MEM_STATE_SHARED_MODIFIED2;
		else
			return ;
	}
	if(rt_meta->state==RtMeta::MEM_STATE_SHARED_MODIFIED2) {
		MergeThreadSet(curr_thd_id,rt_meta);
		MergeLockSet(curr_thd_id,race_event_type,rt_meta);
		if(rt_meta->thread_set.size()==1) {
			rt_meta->state=RtMeta::MEM_STATE_EXCLUSIVE2;
			InitializeLockSet(curr_thd_id,race_event_type,rt_meta);
		}
		else {
			if(race_event_type==RACE_EVENT_WRITE && rt_meta->writer_lock_set.Empty())
				rt_meta->state=RtMeta::MEM_STATE_RACY;
			else if(race_event_type==RACE_EVENT_READ && 
				rt_meta->writer_lock_set.Empty() && rt_meta->reader_lock_set.Empty())	
				rt_meta->state=RtMeta::MEM_STATE_RACY;
		}
			
	}
}

void RaceTrack::MergeThreadSet(thread_t curr_thd_id,RtMeta *rt_meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	for(RtMeta::ThreadSet::iterator it=rt_meta->thread_set.begin();
		it!=rt_meta->thread_set.end();) {
		if(it->second<=curr_vc->GetClock((*it).first))
			rt_meta->thread_set.erase(it++);
		else
			it++;
	}
	rt_meta->thread_set[curr_thd_id]=curr_vc->GetClock(curr_thd_id);
}

void RaceTrack::MergeLockSet(thread_t curr_thd_id,RaceEventType race_event_type,
	RtMeta *rt_meta)
{
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	if(race_event_type==RACE_EVENT_READ) {
		rt_meta->reader_lock_set.Merge(curr_reader_lock_set);
		rt_meta->writer_lock_set.Merge(curr_reader_lock_set);
	}
	else if(race_event_type==RACE_EVENT_WRITE) {
		rt_meta->writer_lock_set.JoinAndMerge(&rt_meta->reader_lock_set,
			curr_writer_lock_set);
	}
}

void RaceTrack::InitializeLockSet(thread_t curr_thd_id,RaceEventType race_event_type,
	RtMeta *rt_meta)
{
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	rt_meta->reader_lock_set.Clear();
	rt_meta->writer_lock_set.Clear();
	if(race_event_type==RACE_EVENT_READ && curr_reader_lock_set) {
		rt_meta->reader_lock_set=*curr_reader_lock_set;
	}
	else if(race_event_type==RACE_EVENT_WRITE && curr_writer_lock_set) {
		rt_meta->writer_lock_set=*curr_writer_lock_set;	
	}	
}

LockSet *RaceTrack::GetWriterLockSet(thread_t curr_thd_id)
{
	LockSet *curr_lock_set=NULL;
	LockSetTable::iterator it;
	if((it=writer_lock_set_table_.find(curr_thd_id))==writer_lock_set_table_.end()
		|| writer_lock_set_table_[curr_thd_id]==NULL) {
		curr_lock_set=new LockSet;
		writer_lock_set_table_[curr_thd_id]=curr_lock_set;
	}
	else
		curr_lock_set=it->second;
	return curr_lock_set;
}


LockSet *RaceTrack::GetReaderLockSet(thread_t curr_thd_id)
{
	LockSet *curr_lock_set=NULL;
	LockSetTable::iterator it;
	if((it=reader_lock_set_table_.find(curr_thd_id))==reader_lock_set_table_.end()
		|| reader_lock_set_table_[curr_thd_id]==NULL) {
		curr_lock_set=new LockSet;
		reader_lock_set_table_[curr_thd_id]=curr_lock_set;
	}
	else
		curr_lock_set=it->second;
	return curr_lock_set;
}

void RaceTrack::ProcessFree(Meta *meta)
{
	RtMeta *rt_meta=dynamic_cast<RtMeta *>(meta);
	DEBUG_ASSERT(rt_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && rt_meta->racy) {
		for(RtMeta::InstSet::iterator it=rt_meta->race_inst_set.begin();
			it!=rt_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	delete rt_meta;
}

}//namespace race