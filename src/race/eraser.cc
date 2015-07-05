#include "race/eraser.h"
#include "core/log.h"
namespace race {

Eraser::Eraser():track_racy_inst_(false)
{}

Eraser::~Eraser() 
{
	for(LockSetTable::iterator it=reader_lock_set_table_.begin();
		it!=reader_lock_set_table_.end();it++) 
		delete it->second;
	for(LockSetTable::iterator it=writer_lock_set_table_.begin();
		it!=writer_lock_set_table_.end();it++) 
		delete it->second;
}

void Eraser::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_eraser",
		"whether enable the eraser data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool Eraser::Enabled()
{
	return knob_->ValueBool("enable_eraser");
}

void Eraser::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

Detector::Meta * Eraser::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new EraserMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void Eraser::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
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


void Eraser::AfterPthreadMutexTryLock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Eraser::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
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

void Eraser::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
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

void Eraser::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	lock_reference_map_[addr]=1;
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
	// INFO_FMT_PRINT("reader_lock_set:%s\n",curr_reader_lock_set->ToString().c_str());
	// INFO_FMT_PRINT("writer_lock_set:%s\n",curr_writer_lock_set->ToString().c_str());
}

void Eraser::AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Eraser::AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t addr,int ret_val)
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockWrlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Eraser::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
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

void Eraser::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	EraserMeta *eraser_meta=dynamic_cast<EraserMeta *>(meta);
	DEBUG_ASSERT(eraser_meta);
	//having racy
	if(eraser_meta->racy)
		return ;
	//consider locks only
	UpdateMemoryState(curr_thd_id,RACE_EVENT_READ,eraser_meta);

	if(eraser_meta->state==EraserMeta::MEM_STATE_READ_SHARED || 
		eraser_meta->state==EraserMeta::MEM_STATE_SHARED_MODIFIED) {

		LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
		LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
		//
		// INFO_PRINT("READ :read shared or shared modified\n");
		// INFO_FMT_PRINT("eraser_meta->writer_lock_set:%s\n",
		// 	eraser_meta->writer_lock_set.ToString().c_str());
		// INFO_FMT_PRINT("eraser_meta->reader_lock_set:%s\n",
		// 	eraser_meta->reader_lock_set.ToString().c_str());
		// INFO_FMT_PRINT("curr_reader_lock_set:%s\n",
		// 	curr_reader_lock_set->ToString().c_str());

		eraser_meta->reader_lock_set.Merge(curr_reader_lock_set);
		eraser_meta->writer_lock_set.Merge(curr_writer_lock_set);

		if(eraser_meta->state==EraserMeta::MEM_STATE_SHARED_MODIFIED && 
			eraser_meta->reader_lock_set.Empty() && 
			eraser_meta->writer_lock_set.Empty() &&
			eraser_meta->reader_lock_set.Disjoint(curr_writer_lock_set) &&
			eraser_meta->writer_lock_set.Disjoint(curr_reader_lock_set)) {

			//PrintDebugRaceInfo("ERASER",WRITETOREAD,eraser_meta,curr_thd_id,inst);

			eraser_meta->racy=true;
			//report all potiential races
			//eraser doesn't know which is conflict with current thread's read
			EraserMeta::InstMap::iterator it=eraser_meta->writer_inst_table.begin();
			for(;it!=eraser_meta->writer_inst_table.end(); it++) {
				if(it->first==curr_thd_id)
					continue;
				ReportRace(eraser_meta,it->first,it->second,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);
			}
		}

		//INFO_PRINT("READ END:read shared or shared modified\n");
	}

	eraser_meta->reader_inst_table[curr_thd_id]=inst;
	if(track_racy_inst_)
		eraser_meta->race_inst_set.insert(inst);
}


void Eraser::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	EraserMeta *eraser_meta=dynamic_cast<EraserMeta *>(meta);
	DEBUG_ASSERT(eraser_meta);
	//having racy
	if(eraser_meta->racy)
		return ;

	//consider locks only
	UpdateMemoryState(curr_thd_id,RACE_EVENT_WRITE,eraser_meta);
	if(eraser_meta->state==EraserMeta::MEM_STATE_READ_SHARED || 
		eraser_meta->state==EraserMeta::MEM_STATE_SHARED_MODIFIED) {
		LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
		LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];

		// INFO_PRINT("WRITE:read shared or shared modified\n");
		// INFO_FMT_PRINT("eraser_meta->writer_lock_set:%s\n",
		// 	eraser_meta->writer_lock_set.ToString().c_str());
		// INFO_FMT_PRINT("eraser_meta->reader_lock_set:%s\n",
		// 	eraser_meta->reader_lock_set.ToString().c_str());
		// INFO_FMT_PRINT("curr_writer_lock_set:%s\n",
		// 	curr_writer_lock_set->ToString().c_str());
		
		//current thread's writer lock set intersect with 
		//shared variable's writer and  reader lock sets
		eraser_meta->writer_lock_set.JoinAndMerge(&eraser_meta->reader_lock_set,
			curr_writer_lock_set);

		if(eraser_meta->state==EraserMeta::MEM_STATE_SHARED_MODIFIED && 
			eraser_meta->writer_lock_set.Empty()) {

			//PrintDebugRaceInfo("ERASER",WRITETOWRITE,eraser_meta,curr_thd_id,inst);

			eraser_meta->racy=true;
			//report all potiential races
			//eraser doesn't know which is conflict with current thread's write
			EraserMeta::InstMap::iterator it=eraser_meta->writer_inst_table.begin();
			for(;it!=eraser_meta->writer_inst_table.end() ;it++) {
				if(it->first==curr_thd_id)
					continue;
				//INFO_PRINT("write and write race\n");
				ReportRace(eraser_meta,it->first,it->second,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
		//
		
		if(eraser_meta->writer_lock_set.Empty() && 
			eraser_meta->state==EraserMeta::MEM_STATE_SHARED_MODIFIED && 
			eraser_meta->writer_lock_set.Disjoint(curr_reader_lock_set)) {

			//PrintDebugRaceInfo("ERASER",READTOWRITE,eraser_meta,curr_thd_id,inst);
			
			eraser_meta->racy=true;
			//report all potiential races
			//eraser doesn't know which is conflict with current thread's write
			EraserMeta::InstMap::iterator it=eraser_meta->reader_inst_table.begin();
			for(;it!=eraser_meta->reader_inst_table.end();it++) {
				if(it->first==curr_thd_id)
					continue;
				ReportRace(eraser_meta,it->first,it->second,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}

		//INFO_PRINT("WRITE END:read shared or shared modified\n");
	}

	eraser_meta->writer_inst_table[curr_thd_id]=inst;
	if(track_racy_inst_)
		eraser_meta->race_inst_set.insert(inst);

}

void Eraser::UpdateMemoryState(thread_t curr_thd_id,RaceEventType race_event_type,
	EraserMeta *eraser_meta)
{
	if(eraser_meta->state==EraserMeta::MEM_STATE_UNKNOWN) {
		if(race_event_type==RACE_EVENT_WRITE)
			eraser_meta->state=EraserMeta::MEM_STATE_EXECLUSIVE;
		else
			eraser_meta->state=EraserMeta::MEM_STATE_VIRGIN;
		eraser_meta->thread_id=curr_thd_id;
		if(writer_lock_set_table_[curr_thd_id]!=NULL)
			eraser_meta->writer_lock_set=*writer_lock_set_table_[curr_thd_id];
		if(reader_lock_set_table_[curr_thd_id]!=NULL)
			eraser_meta->reader_lock_set=*reader_lock_set_table_[curr_thd_id];
	} 
	else if(eraser_meta->state==EraserMeta::MEM_STATE_VIRGIN) {
		if(curr_thd_id==eraser_meta->thread_id && 
			race_event_type==RACE_EVENT_WRITE)
			eraser_meta->state=EraserMeta::MEM_STATE_EXECLUSIVE;
		else if(curr_thd_id!=eraser_meta->thread_id) {
			if(race_event_type==RACE_EVENT_READ)
				eraser_meta->state=EraserMeta::MEM_STATE_READ_SHARED;
			else
				eraser_meta->state=EraserMeta::MEM_STATE_SHARED_MODIFIED;
		}
	}
	else if(eraser_meta->state==EraserMeta::MEM_STATE_EXECLUSIVE && 
		curr_thd_id!=eraser_meta->thread_id) {
		if(race_event_type==RACE_EVENT_WRITE)
			eraser_meta->state=EraserMeta::MEM_STATE_SHARED_MODIFIED;
		else if(race_event_type==RACE_EVENT_READ)
			eraser_meta->state=EraserMeta::MEM_STATE_READ_SHARED;
	}
	else if(eraser_meta->state==EraserMeta::MEM_STATE_READ_SHARED &&
		race_event_type==RACE_EVENT_WRITE) {
		eraser_meta->state=EraserMeta::MEM_STATE_SHARED_MODIFIED;
	}

}

LockSet *Eraser::GetWriterLockSet(thread_t curr_thd_id)
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


LockSet *Eraser::GetReaderLockSet(thread_t curr_thd_id)
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

void Eraser::ProcessFree(Meta *meta)
{
	EraserMeta *eraser_meta=dynamic_cast<EraserMeta *>(meta);
	DEBUG_ASSERT(eraser_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && eraser_meta->racy) {
		for(EraserMeta::InstSet::iterator it=eraser_meta->race_inst_set.begin();
			it!=eraser_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	delete eraser_meta;
}

} //namespace race

