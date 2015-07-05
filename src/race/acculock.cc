#include "race/acculock.h"
#include "core/log.h"

namespace race {

AccuLock::AccuLock():track_racy_inst_(false)
{}

AccuLock::~AccuLock()
{
	for(LockSetTable::iterator it=curr_lockset_table_.begin();
		it!=curr_lockset_table_.end();it++) {
		delete it->second;
	}

	for(LockSetTable::iterator it=curr_reader_lockset_table_.begin();
		it!=curr_reader_lockset_table_.end();it++) {
		delete it->second;
	}
}

void AccuLock::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_acculock",
		"whether enable the acculock data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool AccuLock::Enabled()
{
	return knob_->ValueBool("enable_acculock");
}

void AccuLock::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

void AccuLock::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_lockset;
	if(curr_lockset_table_.find(curr_thd_id)==curr_lockset_table_.end() ||
		!curr_lockset_table_[curr_thd_id])
		curr_lockset_table_[curr_thd_id]=new LockSet;
	curr_lockset=curr_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lockset);
	curr_lockset->Add(addr);
}

void AccuLock::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_lockset=curr_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lockset && curr_lockset->Exist(addr));
	curr_lockset->Remove(addr);
	curr_vc_map_[curr_thd_id]->Increment(curr_thd_id);
}

void AccuLock::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	//only for readlock
	LockSet *curr_reader_lockset;
	if(!curr_reader_lockset_table_[curr_thd_id])
		curr_reader_lockset_table_[curr_thd_id]=new LockSet;
	curr_reader_lockset=curr_reader_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_reader_lockset);
	curr_reader_lockset->Add(addr);
}

void AccuLock::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}
void AccuLock::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	//readwrite lock is either in writer lockset or in reader lockset
	ScopedLock lock(internal_lock_);
	bool found=false;
	//search in reader lockset
	if((curr_reader_lockset_table_.find(curr_thd_id)!=
		curr_reader_lockset_table_.end()) && 
		curr_reader_lockset_table_[curr_thd_id]!=NULL) {

		LockSet *curr_reader_lockset=curr_reader_lockset_table_[curr_thd_id];
		curr_reader_lockset->Remove(addr);
		found=true;
	}
	if((curr_lockset_table_.find(curr_thd_id)!=curr_lockset_table_.end()) &&
		curr_lockset_table_[curr_thd_id]!=NULL) {
		LockSet *curr_lockset=curr_lockset_table_[curr_thd_id];
		curr_lockset->Remove(addr);
		found=true;	
	}

	DEBUG_ASSERT(found);
	curr_vc_map_[curr_thd_id]->Increment(curr_thd_id);
}

Detector::Meta *AccuLock::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new AccuLockMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void AccuLock::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process read\n");
	AccuLockMeta *acculock_meta=dynamic_cast<AccuLockMeta*>(meta);
	DEBUG_ASSERT(acculock_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);

	AccuLockMeta::Epoch &writer_epoch=acculock_meta->writer_epls_pair.first;
	LockSet *writer_lockset=&acculock_meta->writer_epls_pair.second;
	//redudant read
	if((acculock_meta->reader_epls_map.find(curr_thd_id)!=
				acculock_meta->reader_epls_map.end() &&
				curr_clk==acculock_meta->reader_epls_map[curr_thd_id].first.second) || 
				(writer_epoch.first==curr_thd_id && writer_epoch.second==curr_clk)) {
		return ;
	}

	//update current thread's read epoch
	acculock_meta->reader_epls_map[curr_thd_id].first.first=curr_thd_id;
	acculock_meta->reader_epls_map[curr_thd_id].first.second=curr_clk;
	
	if(curr_lockset_table_.find(curr_thd_id)!=curr_lockset_table_.end() &&
		curr_lockset_table_[curr_thd_id]!=NULL) {
		//update lockset
		acculock_meta->reader_epls_map[curr_thd_id].second=
			*curr_lockset_table_[curr_thd_id];
	}
	//
	if(curr_reader_lockset_table_.find(curr_thd_id)!=curr_reader_lockset_table_.end() 
		&& curr_reader_lockset_table_[curr_thd_id]!=NULL) {
		acculock_meta->reader_epls_map[curr_thd_id].second.Join(
			curr_reader_lockset_table_[curr_thd_id]);
	}
	// INFO_FMT_PRINT("addr:%lx\n",acculock_meta->addr);
	// INFO_FMT_PRINT("reader_epoch:<%lx,%ld>\n",curr_thd_id,curr_clk);
	// if(curr_lockset_table_[curr_thd_id]) {
	// 	INFO_FMT_PRINT("reader_lockset :%s\n",
	// 		acculock_meta->reader_epls_map[curr_thd_id].second.ToString().c_str());	
	// }
	// INFO_FMT_PRINT("writer_epoch:<%lx,%ld>\n",writer_epoch.first,writer_epoch.second);
	// INFO_FMT_PRINT("writer_lockset :%s\n",writer_lockset->ToString().c_str());

	//write-read race
	bool racy=false;
	if(writer_epoch.first!=curr_thd_id) {
		if(writer_epoch.second > curr_vc->GetClock(writer_epoch.first)) {
			LockSet *curr_lockset=&(acculock_meta->reader_epls_map[curr_thd_id].second);
			// INFO_PRINT("process read\n");	
			// INFO_FMT_PRINT("writer_lockset:%s\n",writer_lockset->ToString().c_str());
			// INFO_FMT_PRINT("curr_lockset:%s\n",curr_lockset->ToString().c_str());
			if(writer_lockset->Disjoint(curr_lockset))
				racy=true;
		}

		// if(!racy && writer_epoch.second >= curr_vc->GetClock(writer_epoch.first)) {
		// 	LockSet *curr_lockset=&(acculock_meta->reader_epls_map[curr_thd_id].second);
		// 	if(writer_lockset->Disjoint(curr_lockset))
		// 		racy=true;
		// }

		if(racy) {

			//PrintDebugRaceInfo("ACCULOCK",READTOWRITE,acculock_meta,curr_thd_id,inst);

			acculock_meta->racy=true;
			thread_t thd_id=writer_epoch.first;
			Inst *writer_inst=acculock_meta->writer_inst_table[thd_id];
			ReportRace(acculock_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
				curr_thd_id,inst,RACE_EVENT_READ);
		}
	}

	acculock_meta->reader_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		acculock_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process read end\n");
}

void AccuLock::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	AccuLockMeta *acculock_meta=dynamic_cast<AccuLockMeta*>(meta);
	DEBUG_ASSERT(acculock_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);

	//redudant write
	if(acculock_meta->writer_epls_pair.first.first==curr_thd_id && 
		acculock_meta->writer_epls_pair.first.second==curr_clk) {
		return ;
	}

	AccuLockMeta::Epoch &writer_epoch=acculock_meta->writer_epls_pair.first;
	LockSet *writer_lockset=&acculock_meta->writer_epls_pair.second;
	// INFO_FMT_PRINT("addr:%lx\n",acculock_meta->addr);
	// INFO_FMT_PRINT("writer_epoch:<%lx,%ld>\n",writer_epoch.first,writer_epoch.second);
	// INFO_FMT_PRINT("writer_lockset :%s\n",writer_lockset->ToString().c_str());
	// INFO_FMT_PRINT("reader epoch size :%ld\n",acculock_meta->reader_epls_map.size());
	// INFO_FMT_PRINT("curr_clk :%ld\n",curr_clk);

	//write-write race
	if(writer_epoch.first!=curr_thd_id && 
		writer_epoch.second > curr_vc->GetClock(writer_epoch.first)) {
		if(curr_lockset_table_.find(curr_thd_id)!=curr_lockset_table_.end() &&
			curr_lockset_table_[curr_thd_id])
			writer_lockset->Merge(curr_lockset_table_[curr_thd_id]);
		else
			writer_lockset->Clear();
		if(writer_lockset->Empty()) {

			//PrintDebugRaceInfo("ACCULOCK",WRITETOWRITE,acculock_meta,curr_thd_id,inst);

			acculock_meta->racy=true;
			thread_t thd_id=writer_epoch.first;
			Inst *writer_inst=acculock_meta->writer_inst_table[thd_id];
			ReportRace(acculock_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_WRITE);
		}
	}
	else {
		if(curr_lockset_table_.find(curr_thd_id)!=curr_lockset_table_.end() &&
			curr_lockset_table_[curr_thd_id]) {
			acculock_meta->writer_epls_pair.second=
											*curr_lockset_table_[curr_thd_id];
		}
	}
	
	//update epoch
	acculock_meta->writer_epls_pair.first.first=curr_thd_id;
	acculock_meta->writer_epls_pair.first.second=curr_clk;
	//read-write races
	AccuLockMeta::ThreadEpLsMap::iterator it=acculock_meta->reader_epls_map.begin();
	for(;it!=acculock_meta->reader_epls_map.end();it++) {
		AccuLockMeta::Epoch &reader_epoch=it->second.first;
		LockSet *reader_lockset=&it->second.second;

		// INFO_FMT_PRINT("reader_epoch:<%lx,%ld>\n",reader_epoch.first,reader_epoch.second);
		// INFO_FMT_PRINT("reader_lockset :%s\n",reader_lockset->ToString().c_str());
		// INFO_FMT_PRINT("curr_clk :%ld\n",curr_clk);

		if(reader_epoch.first!=curr_thd_id && 
			reader_epoch.second>curr_vc->GetClock(reader_epoch.first)) {
			if(reader_lockset->Disjoint(curr_lockset_table_[curr_thd_id])) {
				
				//PrintDebugRaceInfo("ACCULOCK",READTOWRITE,acculock_meta,curr_thd_id,inst);

				acculock_meta->racy=true;
				thread_t thd_id=reader_epoch.first;
				Inst *reader_inst=acculock_meta->reader_inst_table[thd_id];
				ReportRace(acculock_meta,thd_id,reader_inst,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
	}

	//clear reader epoch lockset
	acculock_meta->reader_epls_map.clear();

	acculock_meta->writer_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		acculock_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process write end\n");
}

void AccuLock::ProcessFree(Meta *meta)
{
	AccuLockMeta *acculock_meta=dynamic_cast<AccuLockMeta *>(meta);
	DEBUG_ASSERT(acculock_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && acculock_meta->racy) {
		for(AccuLockMeta::InstSet::iterator it=acculock_meta->race_inst_set.begin();
			it!=acculock_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}

	delete acculock_meta;
}

}//namespace race