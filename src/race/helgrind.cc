#include "race/helgrind.h"
#include "core/log.h"

namespace race {

Helgrind::Helgrind():track_racy_inst_(false)
{}

Helgrind::~Helgrind()
{
	for(LockSetTable::iterator it=writer_lock_set_table_.begin();
		it!=writer_lock_set_table_.end();it++)
		delete it->second;
	for(LockSetTable::iterator it=reader_lock_set_table_.begin();
		it!=reader_lock_set_table_.end();it++)
		delete it->second;
}

void Helgrind::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_helgrind",
		"whether enable the helgrind data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool Helgrind::Enabled()
{
	return knob_->ValueBool("enable_helgrind");
}

void Helgrind::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

void Helgrind::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	Detector::ThreadStart(curr_thd_id,parent_thd_id);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}

void Helgrind::AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
  	Inst *inst,thread_t childThdId)
{
	Detector::AfterPthreadCreate(currThdId,currThdClk,inst,childThdId);
	create_segment_map_[currThdId]=SEGMENT_NEW;
}

void Helgrind::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,thread_t child_thd_id)
{
	Detector::AfterPthreadJoin(curr_thd_id,curr_thd_clk,inst,child_thd_id);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;	

	for(Meta::Table::iterator it=meta_table_.begin();it!=meta_table_.end();
		it++) {
		HgMeta *hg_meta=dynamic_cast<HgMeta *>(it->second);
		//remove the thread
		if(hg_meta->thread_set.find(child_thd_id)!= hg_meta->thread_set.end())
			hg_meta->thread_set.erase(child_thd_id);
			
		//remove the last operation history
		if(hg_meta->lastop_table.find(child_thd_id)!=hg_meta->lastop_table.end())
			hg_meta->lastop_table.erase(child_thd_id);
		//change to exclusive state

		if(hg_meta->thread_set.size()==1) {
			thread_t thd_id=*(hg_meta->thread_set.begin());
			if(hg_meta->lastop_table.find(thd_id)==hg_meta->lastop_table.end())
				continue;
			//DEBUG_ASSERT(hg_meta->lastop_table[thd_id]);

			if(hg_meta->lastop_table[thd_id]==RACE_EVENT_WRITE)
				hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_WRITE;
			else if(hg_meta->lastop_table[thd_id]==RACE_EVENT_READ)
				hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_READ;
			//update the segment
			hg_meta->lock_set.Clear();
			hg_meta->thread_set.clear();
			//we should ensure the vc not to change,otherwise when we in exclusive
			//may occure some false positives.
			hg_meta->UpdateSegment(hg_meta->segment.vc,thd_id,
				hg_meta->lastop_table[thd_id]);
		}
	}
}

Detector::Meta * Helgrind::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new HgMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void Helgrind::AfterPthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);

	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
}

void Helgrind::BeforePthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//
	curr_writer_lock_set->Remove(addr);
	curr_reader_lock_set->Remove(addr);
}


void Helgrind::AfterPthreadRwlockRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_reader_lock_set && curr_writer_lock_set);
	//reader lock can only protect read access
	curr_reader_lock_set->Add(addr);
	//reader lock reference counting 
	if(lock_reference_map_.find(addr)!=lock_reference_map_.end())
		lock_reference_map_[addr]+=1;
	else
		lock_reference_map_[addr]=1;
}

void Helgrind::AfterPthreadRwlockWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);

	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
	//writer lock refenrence counting
	lock_reference_map_[addr]=1;
}

void Helgrind::BeforePthreadRwlockUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
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


void Helgrind::BeforePthreadCondSignal(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::BeforePthreadCondSignal(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}
   
void Helgrind::BeforePthreadCondBroadcast(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst, address_t addr)
{
	Detector::BeforePthreadCondBroadcast(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;	
}

void Helgrind::BeforePthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	//condition varibale waiting process first release lock and then block
	//this process is atomic so create segment only after the last 
	//synchronization
	Detector::BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,
		mutex_addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}

void Helgrind::AfterPthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	Detector::AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,
		mutex_addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}	

void Helgrind::BeforePthreadCondTimedwait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void Helgrind::AfterPthreadCondTimedwait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void Helgrind::BeforePthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::BeforePthreadBarrierWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}

void Helgrind::AfterPthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::AfterPthreadBarrierWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}

void Helgrind::BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr)
{
	Detector::BeforeSemPost(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}

void Helgrind::AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr)
{
	Detector::AfterSemWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=SEGMENT_NEW;
}


void Helgrind::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process read\n");
	//redudant read:same segment second or more read acccess
	// if(create_segment_map_[curr_thd_id] & SEGMENT_FIRST_READ)
	// 	return ;
	create_segment_map_[curr_thd_id] |= SEGMENT_FIRST_READ;

	HgMeta *hg_meta=dynamic_cast<HgMeta *>(meta);
	DEBUG_ASSERT(hg_meta);

	if(hg_meta->state==HgMeta::MEM_STATE_RACE)
		return ;

	UpdateMemoryState(curr_thd_id,RACE_EVENT_READ,hg_meta,inst);
	// INFO_FMT_PRINT("segment:%s\n",hg_meta->segment.vc.ToString().c_str());
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc_map_[curr_thd_id]->ToString().c_str());
	// INFO_FMT_PRINT("lockset:%s\n",hg_meta->lock_set.ToString().c_str());
	// INFO_FMT_PRINT("state:%d\n",hg_meta->state);
	//racy
	if(hg_meta->state==HgMeta::MEM_STATE_RACE && 
		hg_meta->segment.race_event_type==RACE_EVENT_WRITE &&
		hg_meta->segment.thd_id!=curr_thd_id) {
		
		//PrintDebugRaceInfo("HELGRIND",WRITETOREAD,hg_meta,curr_thd_id,inst);

		hg_meta->racy=true;
		ReportRace(hg_meta,hg_meta->segment.thd_id,hg_meta->inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);	
	}

	if(track_racy_inst_)
		hg_meta->race_inst_set.insert(inst);

	//INFO_PRINT("process read end\n");
}

void Helgrind::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	//redudant write:same segment second or more write acccess
	// if(create_segment_map_[curr_thd_id] & SEGMENT_FIRST_WRITE)
	// 	return ;

	HgMeta *hg_meta=dynamic_cast<HgMeta *>(meta);
	DEBUG_ASSERT(hg_meta);
	create_segment_map_[curr_thd_id] |= SEGMENT_FIRST_WRITE;

	if(hg_meta->state==HgMeta::MEM_STATE_RACE)
		return ;

	UpdateMemoryState(curr_thd_id,RACE_EVENT_WRITE,hg_meta,inst);
	// INFO_FMT_PRINT("segment:%s\n",hg_meta->segment.vc.ToString().c_str());
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc_map_[curr_thd_id]->ToString().c_str());
	// INFO_FMT_PRINT("lock_set:%s\n",hg_meta->lock_set.ToString().c_str());
	// INFO_FMT_PRINT("thread_set size:%ld\n",hg_meta->thread_set.size());
	// INFO_FMT_PRINT("state:%d\n",hg_meta->state);
	//racy
	if(hg_meta->state==HgMeta::MEM_STATE_RACE && 
		hg_meta->segment.race_event_type==RACE_EVENT_WRITE &&
		hg_meta->segment.thd_id!=curr_thd_id) {

		//PrintDebugRaceInfo("HELGRIND",WRITETOWRITE,hg_meta,curr_thd_id,inst);

		hg_meta->racy=true;
		DEBUG_ASSERT(hg_meta->inst);
		ReportRace(hg_meta,hg_meta->segment.thd_id,hg_meta->inst,RACE_EVENT_WRITE,
			curr_thd_id,inst,RACE_EVENT_WRITE);	
	}
	else if(hg_meta->state==HgMeta::MEM_STATE_RACE && 
		hg_meta->segment.race_event_type==RACE_EVENT_READ &&
		hg_meta->segment.thd_id!=curr_thd_id) {
		
		//PrintDebugRaceInfo("HELGRIND",READTOWRITE,hg_meta,curr_thd_id,inst);

		hg_meta->racy=true;
		DEBUG_ASSERT(hg_meta->inst);
		ReportRace(hg_meta,hg_meta->segment.thd_id,hg_meta->inst,RACE_EVENT_READ,
			curr_thd_id,inst,RACE_EVENT_WRITE);	
	}

	if(track_racy_inst_)
		hg_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process write end\n");
}

void Helgrind::ChangeStateExclusiveRead(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst)
{
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	Segment curr_segment(*curr_vc,curr_thd_id,race_event_type);
	
	if(race_event_type==RACE_EVENT_READ) {
		// INFO_FMT_PRINT("exclusive read:0x%lx\n",hg_meta->addr);
		// INFO_FMT_PRINT("curr_segment vc:%s\n",curr_segment.vc.ToString().c_str());
		// INFO_FMT_PRINT("segment vc:%s\n",hg_meta->segment.vc.ToString().c_str());
		//exclusive_read--------->shared_read
		if(!hg_meta->segment.HappensBefore(curr_segment)) {
		//do nothing
			if(hg_meta->segment.thd_id==curr_thd_id)
				return ;
			//kept the original segment
			hg_meta->thread_set.clear();
			hg_meta->thread_set.insert(hg_meta->segment.thd_id);
			hg_meta->thread_set.insert(curr_thd_id);
			hg_meta->state=HgMeta::MEM_STATE_SHARED_READ;
			hg_meta->lock_set.Join(curr_reader_lock_set);
		}
		//exclusive_read--------->exclusive_read
		else {
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
		}
	}
	else if(race_event_type==RACE_EVENT_WRITE) {
		//exclusive_read--------->exclusive_write
		if(hg_meta->segment.HappensBefore(curr_segment)) {
			//do nothing
			if(hg_meta->segment.thd_id==curr_thd_id)
				return ;
			//new segment
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
			hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_WRITE;
		}
		//exclusive_read--------->shared_modified
		else if(curr_writer_lock_set && !curr_writer_lock_set->Empty()) {
			//new segment
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
			hg_meta->state=HgMeta::MEM_STATE_SHARED_MODIFIED;	
			hg_meta->lock_set=*curr_writer_lock_set;
			hg_meta->thread_set.clear();
			hg_meta->thread_set.insert(hg_meta->segment.thd_id);
			hg_meta->thread_set.insert(curr_thd_id);
		}
		//exclusive_read--------->race
		else {
			//only update the state info
			//kept the original info for reporting race
			hg_meta->state=HgMeta::MEM_STATE_RACE;
		}
	}
}

void Helgrind::ChangeStateExclusiveWrite(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst)
{
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	Segment curr_segment(*curr_vc,curr_thd_id,race_event_type);
	
	if(race_event_type==RACE_EVENT_READ) {
		//exclusive_write--------->exclusive_read
		if(hg_meta->segment.HappensBefore(curr_segment)){
			//do nothing
			if(hg_meta->segment.thd_id==curr_thd_id)
				return ;
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
			hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_READ;
		}
		//exclusive_write--------->shared_modified
		else if(curr_reader_lock_set && !curr_reader_lock_set->Empty()) {
			
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
			//thread set
			hg_meta->thread_set.clear();
			hg_meta->thread_set.insert(hg_meta->segment.thd_id);
			hg_meta->thread_set.insert(curr_thd_id);
			hg_meta->state=HgMeta::MEM_STATE_SHARED_MODIFIED;
			hg_meta->lock_set.Join(curr_reader_lock_set);
		}
		else
			hg_meta->state=HgMeta::MEM_STATE_RACE;
	}
	else if(race_event_type==RACE_EVENT_WRITE) {
		//exclusive_write--------->exclusive_write
		if(hg_meta->segment.HappensBefore(curr_segment)) {
			//do nothing
			if(hg_meta->segment.thd_id==curr_thd_id)
				return ;
			//new segment
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
		}
		//exclusive_write--------->shared_modified
		else if(curr_writer_lock_set && !curr_writer_lock_set->Empty()) {
			//new segment
			hg_meta->segment=curr_segment;
			hg_meta->inst=inst;
			hg_meta->state=HgMeta::MEM_STATE_SHARED_MODIFIED;	
			hg_meta->lock_set=*curr_writer_lock_set;
			hg_meta->thread_set.clear();
			hg_meta->thread_set.insert(hg_meta->segment.thd_id);
			hg_meta->thread_set.insert(curr_thd_id);
		}
		//exclusive_write--------->race
		else 
			hg_meta->state=HgMeta::MEM_STATE_RACE;
	}
	// INFO_FMT_PRINT("change exclusive write state:%d,address:0x%lx\n",
	// 	hg_meta->state,hg_meta->addr);
}

void Helgrind::ChangeStateSharedRead(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst)
{
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	Segment curr_segment(*curr_vc,curr_thd_id,race_event_type);
	//shared_read--------->shared_read
	if(race_event_type==RACE_EVENT_READ) {
		hg_meta->lock_set.Merge(curr_reader_lock_set);
		hg_meta->thread_set.insert(curr_thd_id);
	}
	else if(race_event_type==RACE_EVENT_WRITE){
		//shared_read--------->shared_modified
		hg_meta->lock_set.Merge(curr_writer_lock_set);
		if(!hg_meta->lock_set.Empty() || hg_meta->segment.HappensBefore(curr_segment)) {
			//we do not need to update the segment
			hg_meta->state=HgMeta::MEM_STATE_SHARED_MODIFIED;
		}
		//shared_read--------->race
		else {
			//do nothing
			if(hg_meta->segment.thd_id==curr_thd_id)
				return ;
			hg_meta->state=HgMeta::MEM_STATE_RACE;
		}
			
	}
}

void Helgrind::ChangeStateSharedModified(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst)
{
	//INFO_PRINT("shared modified\n");
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	LockSet *curr_write_lock_set=writer_lock_set_table_[curr_thd_id];
	Segment curr_segment(*curr_vc,curr_thd_id,race_event_type);
	// INFO_FMT_PRINT("curr_lock_set:%s\n",curr_write_lock_set->ToString().c_str());
	// INFO_FMT_PRINT("curr_segment vc:%s\n",curr_segment.vc.ToString().c_str());
	// INFO_FMT_PRINT("segment vc:%s\n",hg_meta->segment.vc.ToString().c_str());
	if(race_event_type==RACE_EVENT_READ) {
		//shared_modified--------->exclusive_read
		hg_meta->lock_set.Merge(curr_reader_lock_set);
		if(hg_meta->lock_set.Empty()) {
			if(hg_meta->segment.HappensBefore(curr_segment)) {
				hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_READ;
				hg_meta->segment=curr_segment;	
				hg_meta->inst=inst;
				//exclusive_read do not need lockset protection
				hg_meta->lock_set.Clear();
				hg_meta->thread_set.clear();
			}
			else
				hg_meta->state=HgMeta::MEM_STATE_RACE;
		}
	}
	else if(race_event_type==RACE_EVENT_WRITE) {
		//shared_modified--------->exclusive_write
		hg_meta->lock_set.Merge(curr_write_lock_set);
		if(hg_meta->lock_set.Empty()) {
			if(hg_meta->segment.HappensBefore(curr_segment)) {
				hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_WRITE;
				hg_meta->segment=curr_segment;
				hg_meta->inst=inst;
				//exclusive_read do not need lockset protection
				hg_meta->lock_set.Clear();
				hg_meta->thread_set.clear();
			}
			else
				hg_meta->state=HgMeta::MEM_STATE_RACE;
		}	
	}
	// INFO_FMT_PRINT("state:%d\n",hg_meta->state);
	// INFO_FMT_PRINT("segment event type:%d\n",hg_meta->segment.race_event_type);
	// INFO_FMT_PRINT("segment thd id:%lx,curr_thd_id:%lx\n",hg_meta->segment.thd_id,curr_thd_id);
	//INFO_PRINT("shared modified end\n");
}

void Helgrind::ChangeStateNew(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,VectorClock *curr_vc,Inst *inst)
{
	if(race_event_type==RACE_EVENT_READ)
		hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_READ;
	else if(race_event_type==RACE_EVENT_WRITE)
		hg_meta->state=HgMeta::MEM_STATE_EXCLUSIVE_WRITE;
	hg_meta->inst=inst;
	hg_meta->UpdateSegment(*curr_vc,curr_thd_id,race_event_type);
}

void Helgrind::UpdateMemoryState(thread_t curr_thd_id,RaceEventType race_event_type,
	HgMeta *hg_meta,Inst *inst)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	hg_meta->lastop_table[curr_thd_id]=race_event_type;
	//INFO_FMT_PRINT("update memory state:%d,address:0x%lx\n",hg_meta->state,hg_meta->addr);
	switch(hg_meta->state) {
		case HgMeta::MEM_STATE_NEW :
			return ChangeStateNew(curr_thd_id,race_event_type,
				hg_meta,curr_vc,inst);
		case HgMeta::MEM_STATE_EXCLUSIVE_READ :
			return ChangeStateExclusiveRead(curr_thd_id,race_event_type,
				hg_meta,curr_vc,inst);
		case HgMeta::MEM_STATE_EXCLUSIVE_WRITE :
			return ChangeStateExclusiveWrite(curr_thd_id,race_event_type,
				hg_meta,curr_vc,inst);
		case HgMeta::MEM_STATE_SHARED_READ :
			return ChangeStateSharedRead(curr_thd_id,race_event_type,
				hg_meta,curr_vc,inst);
		case HgMeta::MEM_STATE_SHARED_MODIFIED :
			return ChangeStateSharedModified(curr_thd_id,race_event_type,
				hg_meta,curr_vc,inst);
		default:
			return ;
	}
}

LockSet *Helgrind::GetWriterLockSet(thread_t curr_thd_id)
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

LockSet *Helgrind::GetReaderLockSet(thread_t curr_thd_id)
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

void Helgrind::ProcessFree(Meta *meta)
{
	HgMeta *hg_meta=dynamic_cast<HgMeta *>(meta);
	DEBUG_ASSERT(hg_meta);

	if(track_racy_inst_ && hg_meta->racy) {
		for(HgMeta::InstSet::iterator it=hg_meta->race_inst_set.begin();
			it!=hg_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	delete hg_meta;
}

}