#include "race/fast_track.h"
#include "core/log.h"

namespace race {

FastTrack::FastTrack():track_racy_inst_(false)
{}

FastTrack::~FastTrack()
{}

void FastTrack::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_fast_track",
		"whether enable the fast_track data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool FastTrack::Enabled()
{
	return knob_->ValueBool("enable_fast_track");
}

void FastTrack::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

Detector::Meta *FastTrack::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new FtMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

Detector::MutexMeta *FastTrack::GetMutexMeta(address_t iaddr)
{
	MutexMeta::Table::iterator it=mutex_meta_table_.find(iaddr);
	if(it==mutex_meta_table_.end()) {
		MutexMeta *mutex=new RwlockMeta;
		mutex_meta_table_[iaddr]=mutex;
		return mutex;
	}
	return it->second;	
}

void FastTrack::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	RwlockMeta *meta=dynamic_cast<RwlockMeta *>(GetMutexMeta(addr));
	DEBUG_ASSERT(meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	curr_vc->Join(&meta->vc);
	meta->ref_count++;
	//INFO_FMT_PRINT("after pthread rwlock-rdlock curr_vc:%s\n",curr_vc->ToString().c_str());
}

void FastTrack::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	RwlockMeta *meta=dynamic_cast<RwlockMeta *>(GetMutexMeta(addr));
	DEBUG_ASSERT(meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	curr_vc->Join(&meta->vc);
	DEBUG_ASSERT(meta->ref_count==0);
	meta->ref_count++;	
}

void FastTrack::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	RwlockMeta *meta=dynamic_cast<RwlockMeta *>(GetMutexMeta(addr));
	DEBUG_ASSERT(meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	//update the wait vector clock
	meta->wait_vc.Join(curr_vc);
	meta->ref_count--;
	if(meta->ref_count==0) {
		meta->vc=meta->wait_vc;
		meta->wait_vc.Clear();
	}
	curr_vc->Increment(curr_thd_id);
}

void FastTrack::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process read\n");
	FtMeta *ft_meta=dynamic_cast<FtMeta*>(meta);
	DEBUG_ASSERT(ft_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);
	//same epoch
	// INFO_FMT_PRINT("addr:%lx\n",meta->addr);
	// INFO_FMT_PRINT("reader_epoch-thd:%lx,reader_epoch-clk:%ld\n",
	// 	ft_meta->reader_epoch.first,ft_meta->reader_epoch.second);
	// INFO_FMT_PRINT("curr_thd:%lx,curr_clk:%ld\n",curr_thd_id,curr_clk);
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc->ToString().c_str());
	if(ft_meta->reader_epoch.first==curr_thd_id && 
		ft_meta->reader_epoch.second==curr_clk)
		return ;
	
	//write-read race
	if(ft_meta->writer_epoch.first!=curr_thd_id && 
		(ft_meta->writer_epoch.second > 
			curr_vc->GetClock(ft_meta->writer_epoch.first))) {

		//PrintDebugRaceInfo("FASTTRACK",WRITETOREAD,ft_meta,curr_thd_id,inst);

		ft_meta->racy=true;
		thread_t thd_id=ft_meta->writer_epoch.first;
		DEBUG_ASSERT(ft_meta->writer_inst_table.find(thd_id)!=
			ft_meta->writer_inst_table.end() && 
			ft_meta->writer_inst_table[thd_id]!=NULL);
		Inst *writer_inst=ft_meta->writer_inst_table[thd_id];
		ReportRace(ft_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);
	}
	//read_shared
	if(ft_meta->shared) {
		ft_meta->reader_vc.SetClock(curr_thd_id,curr_clk);
	}
	else {
		//exclusive
		//-same thread
		//-different thread and happens-before current thread
		//-first read
		thread_t &reader_epoch_thd=ft_meta->reader_epoch.first;
		timestamp_t &reader_epoch_clk=ft_meta->reader_epoch.second;

		if((reader_epoch_thd==curr_thd_id && reader_epoch_clk<curr_clk) || 
			reader_epoch_thd==0 || (reader_epoch_thd!=curr_thd_id && 
				reader_epoch_clk<=curr_vc->GetClock(reader_epoch_thd))) {

			reader_epoch_thd=curr_thd_id;
			reader_epoch_clk=curr_clk;
		}
		else {
			//INFO_PRINT("read share\n");
			ft_meta->reader_vc.SetClock(curr_thd_id,curr_clk);
			ft_meta->reader_vc.SetClock(reader_epoch_thd,reader_epoch_clk);
			ft_meta->shared=true;
		}
	}
	ft_meta->reader_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		ft_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process read end\n");
}

void FastTrack::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{

	//INFO_PRINT("process write\n");
	FtMeta *ft_meta=dynamic_cast<FtMeta*>(meta);
	DEBUG_ASSERT(ft_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);
	// INFO_FMT_PRINT("addr:%lx\n",meta->addr);
	// INFO_FMT_PRINT("writer_epoch-thd:%lx,writer_epoch-clk:%ld\n",
	// 	ft_meta->writer_epoch.first,ft_meta->writer_epoch.second);
	// INFO_FMT_PRINT("curr_thd:%lx,curr_clk:%ld\n",curr_thd_id,curr_clk);
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc->ToString().c_str());

	//same epoch
	if(ft_meta->writer_epoch.first==curr_thd_id && 
		ft_meta->writer_epoch.second==curr_clk)
		return ;

	//write-write race
	if(ft_meta->writer_epoch.first!=curr_thd_id &&
		(ft_meta->writer_epoch.second > 
			curr_vc->GetClock(ft_meta->writer_epoch.first))) {

		//PrintDebugRaceInfo("FASTTRACK",WRITETOWRITE,ft_meta,curr_thd_id,inst);

		ft_meta->racy=true;
		thread_t thd_id=ft_meta->writer_epoch.first;
		DEBUG_ASSERT(ft_meta->writer_inst_table.find(thd_id)!=
			ft_meta->writer_inst_table.end() && 
			ft_meta->writer_inst_table[thd_id]!=NULL);
		Inst *writer_inst=ft_meta->writer_inst_table[thd_id];
		ReportRace(ft_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_WRITE);
	}

	//read-write race
	//read occurs only in a thread
	if(!ft_meta->shared) {
		if(ft_meta->reader_epoch.first!=curr_thd_id &&
			(ft_meta->reader_epoch.second > 
				curr_vc->GetClock(ft_meta->reader_epoch.first)) ) {

			//PrintDebugRaceInfo("FASTTRACK",READTOWRITE,ft_meta,curr_thd_id,inst);

			ft_meta->racy=true;
			thread_t thd_id=ft_meta->reader_epoch.first;
			DEBUG_ASSERT(ft_meta->reader_inst_table.find(thd_id)!=
				ft_meta->reader_inst_table.end() && 
				ft_meta->reader_inst_table[thd_id]!=NULL);
			Inst *reader_inst=ft_meta->reader_inst_table[thd_id];
			ReportRace(ft_meta,thd_id,reader_inst,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_WRITE);		
		}
	}
	//read occurs in multi threads
	else {
		for(ft_meta->reader_vc.IterBegin();!ft_meta->reader_vc.IterEnd();
			ft_meta->reader_vc.IterNext()) {
			thread_t thd_id=ft_meta->reader_vc.IterCurrThd();
			timestamp_t clk=ft_meta->reader_vc.IterCurrClk();
			if(curr_thd_id!=thd_id && clk>curr_vc->GetClock(thd_id)) {

				//PrintDebugRaceInfo("FASTTRACK",READTOWRITE,ft_meta,curr_thd_id,inst);

				ft_meta->racy=true;

				DEBUG_ASSERT(ft_meta->reader_inst_table.find(thd_id)!=
					ft_meta->reader_inst_table.end() && 
					ft_meta->reader_inst_table[thd_id]!=NULL);
				Inst *reader_inst=ft_meta->reader_inst_table[thd_id];
				ReportRace(ft_meta,thd_id,reader_inst,RACE_EVENT_READ,
					curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}

		//discard the read vc
		ft_meta->reader_vc.Clear();
		ft_meta->shared=false;
		ft_meta->reader_epoch.first=ft_meta->reader_epoch.second=0;
	}
	
	//update writer-epoch
	ft_meta->writer_epoch.first=curr_thd_id;
	ft_meta->writer_epoch.second=curr_clk;

	ft_meta->writer_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		ft_meta->race_inst_set.insert(inst);

	//INFO_PRINT("process write end\n");
}

//
void FastTrack::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,thread_t child_thd_id)
{
	//INFO_PRINT("pthread join \n");
	Detector::AfterPthreadJoin(curr_thd_id,curr_thd_clk,inst,child_thd_id);
	//remove the reader vector clock's entrys
	for(Meta::Table::iterator it=meta_table_.begin();it!=meta_table_.end();
		it++) {
		FtMeta *ft_meta=dynamic_cast<FtMeta*>(it->second);
		if(ft_meta->reader_vc.Find(child_thd_id)) {
			ft_meta->reader_vc.Erase(child_thd_id);
			if(ft_meta->reader_vc.Size()<=1)
				//thread local
				ft_meta->shared=false;
		}
	}
	//INFO_PRINT("pthread join end\n");
}

void FastTrack::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	Detector::AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
	//merge the reader vector clock
	// curr_thd_clk=curr_vc_map_[curr_thd_id]->GetClock(curr_thd_id);
	// for(Meta::Table::iterator it=meta_table_.begin();it!=meta_table_.end();
	// 	it++) {
	// 	FtMeta *ft_meta=dynamic_cast<FtMeta*>(it->second);
	// 	VectorClock tmp;
	// 	for(ft_meta->reader_vc.IterBegin();!ft_meta->reader_vc.IterEnd();
	// 		ft_meta->reader_vc.IterNext()) {
	// 		thread_t thd_id=ft_meta->reader_vc.IterCurrThd();
	// 		timestamp_t clk=ft_meta->reader_vc.IterCurrClk();
	// 		//exclude
	// 		if(curr_thd_clk>clk)
	// 			continue;
	// 		tmp.SetClock(thd_id,clk);
	// 	}
	// 	ft_meta->reader_vc=tmp;
	// 	if(ft_meta->reader_vc.Size()<=1)
	// 		ft_meta->shared=false;
	// }
}
void FastTrack::ProcessFree(Meta *meta)
{
	FtMeta *ft_meta=dynamic_cast<FtMeta *>(meta);
	DEBUG_ASSERT(ft_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && ft_meta->racy) {
		for(FtMeta::InstSet::iterator it=ft_meta->race_inst_set.begin();
			it!=ft_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	delete ft_meta;
}

} //namespace race