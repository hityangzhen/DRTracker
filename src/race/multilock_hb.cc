#include "race/multilock_hb.h"
#include "core/log.h"

namespace race {

MultiLockHb::MultiLockHb():track_racy_inst_(false)
{}

MultiLockHb::~MultiLockHb()
{
	for(LockSetTable::iterator it=curr_lockset_table_.begin();
		it!=curr_lockset_table_.end();it++) {
		delete it->second;
	}
}

void MultiLockHb::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_multilock_hb",
		"whether enable the multilock_hb data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool MultiLockHb::Enabled()
{
	return knob_->ValueBool("enable_multilock_hb");
}

void MultiLockHb::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

void MultiLockHb::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	if(cond_wait_db_)
		cond_wait_db_->AddLastestLock(curr_thd_id,addr);
	LockSet *curr_lockset;
	if(curr_lockset_table_.find(curr_thd_id)==curr_lockset_table_.end() ||
		!curr_lockset_table_[curr_thd_id])
		curr_lockset_table_[curr_thd_id]=new LockSet;
	curr_lockset=curr_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lockset);
	curr_lockset->Add(addr);
	
}

void MultiLockHb::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	if(loop_db_) {
		ProcessSRLSync(curr_thd_id,inst);
		//increase the current thread's timestamp
		VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
		curr_vc->Increment(curr_thd_id);
	}
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
		//remove the unactived lock writes meta
		cond_wait_db_->RemoveUnactivedLockWritesMeta(curr_thd_id,
			addr);
	}
	LockSet *curr_lockset=curr_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lockset && curr_lockset->Exist(addr));
	curr_lockset->Remove(addr);
}


void MultiLockHb::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_reader_lockset;
	if(curr_reader_lockset_table_.find(curr_thd_id)==curr_reader_lockset_table_.end()||
		!curr_reader_lockset_table_[curr_thd_id])
		curr_reader_lockset_table_[curr_thd_id]=new LockSet;
	curr_reader_lockset=curr_reader_lockset_table_[curr_thd_id];
	DEBUG_ASSERT(curr_reader_lockset);
	curr_reader_lockset->Add(addr);
}

void MultiLockHb::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void MultiLockHb::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	//readwrite lock is either in writer lockset or in reader lockset
	ScopedLock lock(internal_lock_);

	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
		//remove the unactived lock writes meta
		cond_wait_db_->RemoveUnactivedLockWritesMeta(curr_thd_id,
			addr);
	}
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
}

Detector::Meta *MultiLockHb::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new MlMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void MultiLockHb::update_on_read(timestamp_t curr_clk,thread_t curr_thd,LockSet* curr_lockset,
	MlMeta *ml_meta)
{
	DEBUG_ASSERT(curr_lockset);
	curr_lockset->Join(curr_reader_lockset_table_[curr_thd]);
	
	//INFO_FMT_PRINT("update_on_read read_lockset :%s\n",curr_lockset->ToString().c_str());
	//skip all redudant read accesses
	MlMeta::EpochLockSetPairVector *reader_elsp_vec=NULL;
	MlMeta::ThreadElspVecMap::iterator elspvec_it=ml_meta->reader_elspvec_map.find(curr_thd);
	if(elspvec_it==ml_meta->reader_elspvec_map.end())
		ml_meta->reader_elspvec_map[curr_thd]=new MlMeta::EpochLockSetPairVector;
	DEBUG_ASSERT(ml_meta->reader_elspvec_map[curr_thd]);
	reader_elsp_vec=ml_meta->reader_elspvec_map[curr_thd];
	for(MlMeta::EpochLockSetPairVector::iterator it=reader_elsp_vec->begin();
		it!=reader_elsp_vec->end();it++) {
		//
		if((*it)->first==curr_clk && (*it)->second.SubLockSet(curr_lockset))
			return ;
	}

	MlMeta::EpochLockSetPairVector *writer_elsp_vec=NULL;
	MlMeta::ThreadElspVecMap::iterator it=ml_meta->writer_elspvec_map.find(curr_thd);
	if(it==ml_meta->writer_elspvec_map.end())
		ml_meta->writer_elspvec_map[curr_thd]=new MlMeta::EpochLockSetPairVector;
	DEBUG_ASSERT(ml_meta->writer_elspvec_map[curr_thd]);
	writer_elsp_vec=ml_meta->writer_elspvec_map[curr_thd];

	for(MlMeta::EpochLockSetPairVector::iterator it=writer_elsp_vec->begin();
		it!=writer_elsp_vec->end();it++) {
		//
		if((*it)->first==curr_clk && (*it)->second.SubLockSet(curr_lockset))
			return ;
	}

	//necessary
	reader_elsp_vec->push_back(new MlMeta::EpochLockSetPair(curr_clk,*curr_lockset));
	//reomve the prior read
	for(MlMeta::EpochLockSetPairVector::iterator it=reader_elsp_vec->begin();
		it!=reader_elsp_vec->end()-1;) {
		//current lockset is a sublockset of prior read's
		if( (*it)->first<=curr_clk && 
			curr_lockset->SubLockSet(&((*it)->second)) ) {
			delete *it;
			it=reader_elsp_vec->erase(it);
		}
		else
			it++;
	}	
}

void MultiLockHb::update_on_write(timestamp_t curr_clk,thread_t curr_thd,LockSet* curr_lockset,
	MlMeta *ml_meta)
{
	
	//skip redudant write access
	DEBUG_ASSERT(curr_lockset);
	MlMeta::EpochLockSetPairVector *writer_elsp_vec=NULL;
	MlMeta::ThreadElspVecMap::iterator elspvec_it=ml_meta->writer_elspvec_map.find(curr_thd);
	if(elspvec_it==ml_meta->writer_elspvec_map.end())
		ml_meta->writer_elspvec_map[curr_thd]=new MlMeta::EpochLockSetPairVector;
	DEBUG_ASSERT(ml_meta->writer_elspvec_map[curr_thd]);
	writer_elsp_vec=ml_meta->writer_elspvec_map[curr_thd];

	for(MlMeta::EpochLockSetPairVector::iterator it=writer_elsp_vec->begin();
		it!=writer_elsp_vec->end();it++) {
		//
		if( (*it)->first==curr_clk &&  (*it)->second.SubLockSet(curr_lockset))
			return ;
	}

	writer_elsp_vec->push_back(new MlMeta::EpochLockSetPair(curr_clk,*curr_lockset));
	//remove prior read
	elspvec_it=ml_meta->reader_elspvec_map.find(curr_thd);
	if(elspvec_it!=ml_meta->reader_elspvec_map.end()) {
		MlMeta::EpochLockSetPairVector *reader_elsp_vec=ml_meta->reader_elspvec_map[curr_thd];
		for(MlMeta::EpochLockSetPairVector::iterator it=reader_elsp_vec->begin();
			it!=reader_elsp_vec->end();) {
			if( (*it)->first<=curr_clk &&
				curr_lockset->SubLockSet(&((*it)->second))) {
				delete *it;
				it=reader_elsp_vec->erase(it);
			}
			else
				it++;
		}
	}

	//remove prior write
	for(MlMeta::EpochLockSetPairVector::iterator it=writer_elsp_vec->begin();
		it!=writer_elsp_vec->end()-1;) {
		if( (*it)->first<=curr_clk &&
			curr_lockset->SubLockSet(&((*it)->second))) {
			delete *it;
			it=writer_elsp_vec->erase(it);
		}
		else
			it++;
	}
	//INFO_PRINT("update_on_write end\n");
}

void MultiLockHb::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	MlMeta *ml_meta=dynamic_cast<MlMeta*>(meta);
	DEBUG_ASSERT(ml_meta);
	//process write->spinning read sync
	if(loop_db_) {
		//handle the previous write->spinning read sync
		ProcessSRLSync(curr_thd_id,inst);
		//if the spin thread
		// if(loop_db_ && (loop_db_->SpinRead(inst,RACE_EVENT_READ) ||
		// 	loop_db_->SpinReadCalledFuncThread(curr_thd_id))) {
		// 	//initialize the spin read meta
		// 	loop_db_->SetSpinReadThread(curr_thd_id,inst);
		// 	loop_db_->SetSpinReadAddr(curr_thd_id,ml_meta->addr);
		// 	//keep the call inst
		// 	if(loop_db_->SpinReadCalledFuncThread(curr_thd_id))
		// 		loop_db_->SetSpinReadCallInst(curr_thd_id);
		// }
		ProcessSRLRead(curr_thd_id,inst,ml_meta->addr);
	}
	//process write->cond_wait read sync
	if(cond_wait_db_) {
		//handle the previous signal/broadcast->cond_wait sync firstly
		ProcessCWLSync(curr_thd_id,inst);
		//do the signal/broadcast->cond_wait sync analysis
		// for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		// 	//process cond wait read
		// 	if(ProcessCWLRead(curr_thd_id,inst,iaddr))
		// 		break;
		// }
		ProcessCWLRead(curr_thd_id,inst,ml_meta->addr);
	}
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);	
	if(curr_lockset_table_.find(curr_thd_id)==curr_lockset_table_.end() ||
		curr_lockset_table_[curr_thd_id]==NULL) {
		//trivial lockset
		curr_lockset_table_[curr_thd_id]=new LockSet;	
	}
	//temporary lockset-union of writer_lockset and reader_lockset
	LockSet lock_set=*curr_lockset_table_[curr_thd_id];
	update_on_read(curr_clk,curr_thd_id,&lock_set,ml_meta);

	//write-read race
	MlMeta::ThreadElspVecMap::iterator it=ml_meta->writer_elspvec_map.begin();
	for(;it!=ml_meta->writer_elspvec_map.end();it++) {
		if(it->first==curr_thd_id)
			continue;
		MlMeta::EpochLockSetPairVector *writer_elsp_vec=it->second;
		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);

		for(MlMeta::EpochLockSetPairVector::iterator elsp_it=writer_elsp_vec->begin();
			elsp_it!=writer_elsp_vec->end();elsp_it++) {
			//
			if((*elsp_it)->first>thd_clk) {
				Inst *writer_inst=ml_meta->writer_inst_table[thd_id];
				//spinning read thread
				//do not consider the lockset
				if(loop_db_ && loop_db_->SpinReadThread(curr_thd_id)) {
					loop_db_->SetSpinRelevantWriteThreadAndInst(curr_thd_id,thd_id,
						writer_inst);
					//remote write protected by the common lock
					if(!(*elsp_it)->second.Disjoint(&lock_set)) {
						loop_db_->SetSpinRelevantWriteLocked(curr_thd_id,true);
					}
				}
			}

			if((*elsp_it)->first>thd_clk && 
				(*elsp_it)->second.Disjoint(&lock_set)) {
				// PrintDebugRaceInfo("MULTILOCK_HB",WRITETOREAD,ml_meta,curr_thd_id,inst);

				ml_meta->racy=true;
				Inst *writer_inst=ml_meta->writer_inst_table[thd_id];
				ReportRace(ml_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);
			}
		}
	}

	ml_meta->reader_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		ml_meta->race_inst_set.insert(inst);
}

void MultiLockHb::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	MlMeta *ml_meta=dynamic_cast<MlMeta*>(meta);
	DEBUG_ASSERT(ml_meta);
	//process write->spinning read sync
	if(loop_db_) {
		//handle the previous write->spinning read sync
		ProcessSRLSync(curr_thd_id,inst);
		//if there are shared location modification in called function
		// if(loop_db_ && loop_db_->SpinReadCalledFuncThread(curr_thd_id)) {
		// 	//not spinning read
		// 	if(loop_db_->SpinReadThread(curr_thd_id) && 
		// 		loop_db_->GetSpinReadAddr(curr_thd_id)==ml_meta->addr) {
		// 		loop_db_->SetSpinReadCalledFunc(curr_thd_id,NULL,false);
		// 		//remove the potential spinning read meta
		// 		loop_db_->RemoveSpinReadMeta(curr_thd_id);			
		// 	}
		// }
		ProcessSRLWrite(curr_thd_id,inst,ml_meta->addr);
	}
	//process write->cond_wait read sync
	if(cond_wait_db_) {
		//handle the previous signal/broadcast->cond_wait sync firstly
		ProcessCWLSync(curr_thd_id,inst);
		//do the signal/broadcast->cond_wait sync analysis
		// for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		// 	ProcessLockSignalWrite(curr_thd_id,iaddr);
		// 	ProcessCWLCalledFuncWrite(curr_thd_id,iaddr);
		// }
		ProcessLockSignalWrite(curr_thd_id,ml_meta->addr);
		ProcessCWLCalledFuncWrite(curr_thd_id,ml_meta->addr);
	}

	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);
	if(curr_lockset_table_.find(curr_thd_id)==curr_lockset_table_.end() ||
		curr_lockset_table_[curr_thd_id]==NULL) {
		//trivial lockset
		curr_lockset_table_[curr_thd_id]=new LockSet;	
	}

	update_on_write(curr_clk,curr_thd_id,curr_lockset_table_[curr_thd_id],
		ml_meta);
	//write-write race
	MlMeta::ThreadElspVecMap::iterator it=ml_meta->writer_elspvec_map.begin();
	for(;it!=ml_meta->writer_elspvec_map.end();it++) {
		if(it->first==curr_thd_id)
			continue;
		MlMeta::EpochLockSetPairVector *writer_elsp_vec=it->second;
		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);

		for(MlMeta::EpochLockSetPairVector::iterator elsp_it=writer_elsp_vec->begin();
			elsp_it!=writer_elsp_vec->end();elsp_it++) {

			// INFO_FMT_PRINT("current lockset:%s\n",
			// 	curr_lockset_table_[curr_thd_id]->ToString().c_str());
			// INFO_FMT_PRINT("writer_elsp:%s\n",(*elsp_it)->second.ToString().c_str());

			if((*elsp_it)->first>thd_clk && 
				(*elsp_it)->second.Disjoint(curr_lockset_table_[curr_thd_id])) {

				// PrintDebugRaceInfo("MULTILOCK_HB",WRITETOWRITE,ml_meta,curr_thd_id,inst);

				ml_meta->racy=true;
				Inst *writer_inst=ml_meta->writer_inst_table[thd_id];
				ReportRace(ml_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
	}
	//read-write race
	it=ml_meta->reader_elspvec_map.begin();
	for(;it!=ml_meta->reader_elspvec_map.end();it++) {
		if(it->first==curr_thd_id)
			continue;
		MlMeta::EpochLockSetPairVector *reader_elsp_vec=it->second;
		thread_t thd_id=it->first;
		timestamp_t thd_clk=curr_vc->GetClock(it->first);

		for(MlMeta::EpochLockSetPairVector::iterator elsp_it=reader_elsp_vec->begin();
			elsp_it!=reader_elsp_vec->end();elsp_it++) {
			//parallel
			if((*elsp_it)->first>thd_clk) {
				//spinning read thread
				if(loop_db_ && loop_db_->SpinReadThread(it->first)) {
					loop_db_->SetSpinRelevantWriteThreadAndInst(it->first,curr_thd_id,
						inst);
					//current write is protected by the common lock
					if(!(*elsp_it)->second.Disjoint(curr_lockset_table_[curr_thd_id])) {
						loop_db_->SetSpinRelevantWriteLocked(it->first,true);
					}
				}
			}

			if((*elsp_it)->first>thd_clk && 
				(*elsp_it)->second.Disjoint(curr_lockset_table_[curr_thd_id])) {
				// PrintDebugRaceInfo("MULTILOCK_HB",READTOWRITE,ml_meta,curr_thd_id,inst);
				ml_meta->racy=true;
				Inst *reader_inst=ml_meta->reader_inst_table[thd_id];
				ReportRace(ml_meta,thd_id,reader_inst,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
	}

	ml_meta->writer_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		ml_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process write end\n");
}


void MultiLockHb::ProcessFree(Meta *meta)
{
	MlMeta *ml_meta=dynamic_cast<MlMeta *>(meta);
	DEBUG_ASSERT(ml_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && ml_meta->racy) {
		for(MlMeta::InstSet::iterator it=ml_meta->race_inst_set.begin();
			it!=ml_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}

	MlMeta::ThreadElspVecMap::iterator it=ml_meta->writer_elspvec_map.begin();
	for(;it!=ml_meta->writer_elspvec_map.end();it++) {
		MlMeta::EpochLockSetPairVector *writer_elsp_vec=it->second;
		for(MlMeta::EpochLockSetPairVector::iterator elsp_it=
			writer_elsp_vec->begin();elsp_it!=writer_elsp_vec->end();
			elsp_it++) {
			delete *elsp_it;
		}
		delete writer_elsp_vec;
	}

	it=ml_meta->reader_elspvec_map.begin();
	for(;it!=ml_meta->reader_elspvec_map.end();it++) {
		MlMeta::EpochLockSetPairVector *reader_elsp_vec=it->second;
		for(MlMeta::EpochLockSetPairVector::iterator elsp_it=
			reader_elsp_vec->begin();elsp_it!=reader_elsp_vec->end();
			elsp_it++) {
			delete *elsp_it;
		}
		delete reader_elsp_vec;
	}

	delete ml_meta;
}


}//namespace race