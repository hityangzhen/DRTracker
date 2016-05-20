#include "race/parallel_verifier_ml.h"
#include "core/log.h"
namespace race
{
ParallelVerifierMl::ParallelVerifierMl():prl_vrf_num_(0)
{}

ParallelVerifierMl::~ParallelVerifierMl() 
{
	for(HtyDtcReqQueueTable::iterator iter=htydtc_reqque_table_.begin();
		iter!=htydtc_reqque_table_.end();iter++) {
		delete iter->second;
	}
	//free the thread local store
	for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
		thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
		ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
			iter->second);
		for(PStmtMetasMap::iterator iiter=tls->pstmt_metas_map.begin();
			iiter!=tls->pstmt_metas_map.end();iiter++)
			delete iiter->second;
	}
	for(ThreadSemaphoreMap::iterator iter=thd_smp_map_.begin();iter!=
		thd_smp_map_.end();iter++)
		delete iter->second;
}

bool ParallelVerifierMl::Enabled()
{
	return knob_->ValueBool("parallel_race_verify_ml");
}

void ParallelVerifierMl::Register()
{
	VerifierMl::Register();
	knob_->RegisterBool("parallel_race_verify_ml","whether enable the parallel"
		" race verify with multilock dynamic detection engine","0");
}

void ParallelVerifierMl::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	SysSemaphore *sys_sema=new SysSemaphore(0);
	ThreadLocalStore *tls=new ThreadLocalStore;
	SetThreadData(tls_key_,tls,GetThreadId());
	tls->vc.Increment(curr_thd_id);
	ScopedLock lock(internal_lock_);
	//not the main thread
	if(parent_thd_id!=INVALID_THD_ID) {
		VectorClock *parent_vc=&((ThreadLocalStore *)GetThreadData(tls_key_,
			thd_uid_map_[parent_thd_id]))->vc;
		DEBUG_ASSERT(parent_vc);
		tls->vc.Join(parent_vc);
		parent_vc->Increment(parent_thd_id);
	}
	tls->status=AVAILABLE;
	thd_uid_map_[curr_thd_id]=GetThreadId();
	//thread semaphore
	if(thd_smp_map_.find(curr_thd_id)==thd_smp_map_.end()) {
		// thd_smp_map_[curr_thd_id]=pin_sema;
		// thd_smp_map_[curr_thd_id]->Init();
		thd_smp_map_[curr_thd_id]=sys_sema;
	}
	//all threads are available at the beginning
	avail_thd_set_.insert(curr_thd_id);
}

void ParallelVerifierMl::ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk)
{
	ScopedLock lock(internal_lock_);
	//free the semaphore
	delete thd_smp_map_[curr_thd_id];
	thd_smp_map_.erase(curr_thd_id);
	//
	avail_thd_set_.erase(curr_thd_id);
	//must remove from postpone thread set to make current thread exit
	if(pp_thd_set_.find(curr_thd_id)!=pp_thd_set_.end())
		pp_thd_set_.erase(curr_thd_id);
	if(avail_thd_set_.empty())
		ChooseRandomThreadAfterAllUnavailable();
}

void ParallelVerifierMl::AfterPthreadCreate(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,thread_t child_thd_id)
{}
	
void ParallelVerifierMl::AfterPthreadJoin(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *curr_tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	VectorClock *curr_vc=&curr_tls->vc;
	ThreadLocalStore *child_tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[child_thd_id]);
	VectorClock *child_vc=&child_tls->vc;
	curr_vc->Join(child_vc);
	curr_vc->Increment(curr_thd_id);
	UnblockThread(curr_thd_id);
}

void ParallelVerifierMl::BeforeMemRead(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,size_t size)
{
	if(FilterAccess(addr))
		return ;
	VerifyRequest *vrf_req=new VerifyRequest(curr_thd_id,inst,addr,size,
		RACE_EVENT_READ);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	PushVerifyRequest(vrf_req,tls);
	PostponeThread(curr_thd_id);
}

void ParallelVerifierMl::BeforeMemWrite(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,size_t size)
{
	if(FilterAccess(addr))
		return ;
	VerifyRequest *vrf_req=new VerifyRequest(curr_thd_id,inst,addr,size,
		RACE_EVENT_WRITE);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	PushVerifyRequest(vrf_req,tls);
	PostponeThread(curr_thd_id);
}

void ParallelVerifierMl::BeforePthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	BarrierMeta *barrier_meta=GetBarrierMeta(addr);
	BlockThread(curr_thd_id);
	//set the vector clock
	barrier_meta->vc.Join(&tls->vc);
	barrier_meta->ref++;
	//
	if(barrier_meta->ref!=barrier_meta->count && avail_thd_set_.empty()) {
		// if(!pp_thd_set_.empty())
			ChooseRandomThreadAfterAllUnavailable();
		// else go through
	}
}

void ParallelVerifierMl::AfterPthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	BarrierMeta *barrier_meta=GetBarrierMeta(addr);
	UnblockThread(curr_thd_id);
	tls->vc.Join(&barrier_meta->vc);
	tls->vc.Increment(curr_thd_id);
	if(--(barrier_meta->ref)==0)
		barrier_meta->vc.Clear();
}

void ParallelVerifierMl::BeforePthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
	address_t mutex_addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	//handle unlock
	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
	ProcessPreMutexUnlock(curr_thd_id,mutex_meta);
	ProcessPostMutexUnlock(curr_thd_id,mutex_meta);
	//
	CondMeta *cond_meta=GetCondMeta(cond_addr);
	BlockThread(curr_thd_id);
	cond_meta->wait_table[curr_thd_id]=tls->vc;

	if(avail_thd_set_.empty()) {
		// if(!pp_thd_set_.empty()) 
			ChooseRandomThreadAfterAllUnavailable();
		// else go through
	}
}

void ParallelVerifierMl::AfterPthreadCondWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	CondMeta *cond_meta=GetCondMeta(cond_addr);
	//clear from the wait table
	CondMeta::ThreadVectorClockMap::iterator witer=
		cond_meta->wait_table.find(curr_thd_id);
	DEBUG_ASSERT(witer!=cond_meta->wait_table.end());
	cond_meta->wait_table.erase(witer);
	//clear the signal info
	CondMeta::ThreadVectorClockMap::iterator siter=
		cond_meta->signal_table.find(curr_thd_id);
	if(siter!=cond_meta->signal_table.end()) {
		tls->vc.Join(&siter->second);
		cond_meta->signal_table.erase(siter);
	}
	//
	UnblockThread(curr_thd_id);
	tls->vc.Increment(curr_thd_id);
	//handle lock
	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
	ProcessPreMutexLock(curr_thd_id,mutex_meta);
	ProcessPostMutexLock(curr_thd_id,mutex_meta);
}

void ParallelVerifierMl::BeforeSemPost(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	SemMeta *sem_meta=GetSemMeta(addr);

	sem_meta->vc.Join(&tls->vc);
	//this step we move from the AfterSemPost to BeforeSemPost
	//and this will not affect the correctness
	tls->vc.Increment(curr_thd_id);
	sem_meta->count++;
}

void ParallelVerifierMl::AfterSemWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	SemMeta *sem_meta=GetSemMeta(addr);
	UnblockThread(curr_thd_id);
	tls->vc.Join(&sem_meta->vc);
	tls->vc.Increment(curr_thd_id);
}

void ParallelVerifierMl::AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,
	timestamp_t curr_thd_clk,RaceEventType type,Inst *inst,PStmt *s)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	MlMetaSnapshot *mlmeta_ss=new MlMetaSnapshot(curr_thd_clk,type,
		inst,s);
	mlmeta_ss->rd_ls=tls->rd_ls;
	mlmeta_ss->wr_ls=tls->wr_ls;
	meta->AddMetaSnapshot(curr_thd_id,mlmeta_ss);
}

//each thread arrive the pstmt should be postponed
void ParallelVerifierMl::PostponeThread(thread_t curr_thd_id)
{
	InternalLock();
	if(avail_thd_set_.size()==1 && pp_thd_set_.empty())
		goto fini;
	pp_thd_set_.insert(curr_thd_id);
	avail_thd_set_.erase(curr_thd_id);
	if(avail_thd_set_.empty())
		ChooseRandomThreadAfterAllUnavailable();
	InternalUnlock();
	//current thread is being postponed
	{
		ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
			thd_uid_map_[curr_thd_id]);
		tls->status=BEING_POSTPONED;
	}
	//semahore wait
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME,&ts);
	ts.tv_sec+=2;
	thd_smp_map_[curr_thd_id]->TimedWait(&ts);

	//after waking up
	InternalLock();
	pp_thd_set_.erase(curr_thd_id);
	avail_thd_set_.insert(curr_thd_id);
fini:
	//bring the postponed thread's metas into history metas
	ClearPostponedThreadMetas(curr_thd_id);
	InternalUnlock();
}

void ParallelVerifierMl::ProcessReadOrWrite(VerifyRequest *req)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[req->thd_id]);
	//prevent the previous remainder metas
	tls->pp_metas.clear();
	ProcessReadOrWrite(req->thd_id,req->inst,req->addr,req->size,req->type);
}

/**
  * Get the parters of the current pstmt which are involved in the pstmt
  * pair data base.
  * This function need synchronization.
  * @param first_pstmts A set contains the parter pstmt
  * @return the pstmt or NULL if it is not expected
  */
PStmt *ParallelVerifierMl::GetFirstPStmtSet(thread_t curr_thd_id,Inst *inst,
	PStmtSet &first_pstmts)
{
	std::string file_name=inst->GetFileName();
	int line=inst->GetLine();

	ScopedLock lock(internal_lock_);
	PStmt *pstmt=prace_db_->GetPStmt(file_name,line);
	if(pstmt==NULL) {
		WakeUpPostponeThread(curr_thd_id);
		return NULL;
	}
	// Traverse the threads' tls to find the parter of current pstmt
	for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
		thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
		if(iter->first==curr_thd_id)
			continue;
		ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
			iter->second);
		// Find the parter pstmt in the thread accessed pstmt set
		for(PStmtMetasMap::iterator iiter=tls->pstmt_metas_map.begin();
			iiter!=tls->pstmt_metas_map.end();iiter++) {
			if(prace_db_->SecondPotentialStatement(iiter->first,pstmt)) {
				first_pstmts.insert(iiter->first);
			}
		}
	}
	return pstmt;
}

void ParallelVerifierMl::ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,
	address_t addr,size_t size,RaceEventType type)
{
	// INFO_FMT_PRINT("===========process read or write start:inst:[%s]============\n",
	// 	inst->ToString().c_str());
	//get the potential statement
	// std::string file_name=inst->GetFileName();
	// int line=inst->GetLine();
	// InternalLock();
	// PStmt *pstmt=prace_db_->GetPStmt(file_name,line);
	// if(pstmt==NULL) {
	// 	WakeUpPostponeThread(curr_thd_id);
	// 	InternalUnlock();
	// 	return ;
	// }
	// address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	// address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);
	// /* Traverse the threads' tls to find the parter of current pstmt */
	// PStmtSet first_pstmts;
	// for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
	// 	thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
	// 	if(iter->first==curr_thd_id)
	// 		continue;
	// 	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
	// 		iter->second);
	// 	for(PStmtMetasMap::iterator iiter=tls->pstmt_metas_map.begin();
	// 		iiter!=tls->pstmt_metas_map.end();iiter++) {
	// 		if(prace_db_->SecondPotentialStatement(iiter->first,pstmt)) {
	// 			first_pstmts.insert(iiter->first);
	// 		}
	// 	}
	// }
	// InternalUnlock();

	PStmtSet first_pstmts;
	PStmt *pstmt=GetFirstPStmtSet(curr_thd_id,inst,first_pstmts);
	if(pstmt==NULL)
		return ;
	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	// timestamp_t curr_thd_clk=tls->vc.GetClock(curr_thd_id);
	SyncObject *sync_obj=GetVerifyRequest()->sync_obj;
	timestamp_t curr_thd_clk=sync_obj->vc.GetClock(curr_thd_id);
	// First accessed parter stmt of the pstmt pair
	if(first_pstmts.empty()) {
		bool redudant_flag=true;
		MAP_KEY_NOTFOUND_NEW(tls->pstmt_metas_map,pstmt,MetaSet);
		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			// Only the verification thread accesses the meta,don't need sync.
			Meta *meta=GetMeta(iaddr);
			DEBUG_ASSERT(meta);
			tls->pp_metas.insert(meta);
			tls->pstmt_metas_map[pstmt]->insert(meta);
			/* We assume that same inst and the same epoch in current threads'
			   recent snapshots means that the access is redudant. */ 
			if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst))
				continue;
			//add snapshot
			AddMetaSnapshot(meta,curr_thd_id,curr_thd_clk,type,inst,pstmt);
			redudant_flag=false;
		}
		//redudant access
		if(redudant_flag) {
			tls->status=AVAILABLE;
			WakeUpPostponeThread(curr_thd_id);
		}
		else {
			// INFO_FMT_PRINT("=========first pstmt postponed:[%s]=========\n",
			// 	inst->ToString().c_str());
			/* Application threads may be concurrent executed with the verification
			   thread. Consider the scenario that application thread has waked up
			   in advance, and current verify request is being processed. So the
			   pp_metas will be filled again and is not added into the hy_metas. */
			ClearWhenVerifyRequestInvalid(tls);
		}
	} 
	else {
		bool redudant_flag=true;
		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			Meta *meta=GetMeta(iaddr);
			DEBUG_ASSERT(meta);
			if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst))
				continue;
			redudant_flag=false;
		}
		//accumulate postponed threads
		std::map<thread_t,bool> pp_thd_map;
		VERIFY_RESULT res=NONE_SHARED;
		if(!redudant_flag) {
			// Count the sent historical request
			for(PStmtSet::iterator iter=first_pstmts.begin();iter!=
				first_pstmts.end();iter++) {
				PStmt *first_pstmt=*iter;
				res=WaitVerification(start_addr,end_addr,first_pstmt,pstmt,inst,
					curr_thd_id,type,pp_thd_map);
			}
			// End the use of sync obj.
			DistributeEndOfSyncObj();
		}
		if(!pp_thd_map.empty())
			HandleRace(pp_thd_map,curr_thd_id);
		else {
			if(res==REDUDANT || res==NONE_SHARED) {
				tls->status=AVAILABLE;
				WakeUpPostponeThread(curr_thd_id);
			}
			else
				HandleNoRace(curr_thd_id);
		}
	}
	// INFO_FMT_PRINT("===========process read or write end============\n");
}

void ParallelVerifierMl::WakeUpPostponeThread(thread_t thd_id)
{
	if(thd_smp_map_.find(thd_id)!=thd_smp_map_.end())
		thd_smp_map_[thd_id]->Post();
}

void ParallelVerifierMl::HandleRace(std::map<thread_t,bool> &pp_thd_map,
	thread_t curr_thd_id)
{
	PostponeThreadSet pp_thds;
	//keep up threads
	PostponeThreadSet keep_up_thds;
	for(std::map<thread_t,bool>::iterator iter=pp_thd_map.begin();
		iter!=pp_thd_map.end();iter++) {
		//pp_thd_map may exist current thread 
		if(iter->first!=curr_thd_id) {
			pp_thds.insert(iter->first);
			if(iter->second==false)
				keep_up_thds.insert(iter->first);
		}
	}
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	//wake up postpone thread set,postpone current thread
	if(!RandomBool()) {
		WakeUpPostponeThreadSet(pp_thds);
		/* The pstmt current thread accessed has been fully verified and 
		   current thread should not be continuously postponed */
		if(pp_thd_map.find(curr_thd_id)!=pp_thd_map.end()) {
			WakeUpPostponeThread(curr_thd_id);
			tls->status=AVAILABLE;
		}
		else //current thread should keep the postponed status
			return ;
	}
	/* Wake up the threads being postponed in pstmts which are fully verified
	   and current thread */ 
	else { 
		WakeUpPostponeThreadSet(keep_up_thds);
		WakeUpPostponeThread(curr_thd_id);
		tls->status=AVAILABLE;
	}
}

/** 
  * This function is invoked when the pstmt pairs are read-shared,
  * we do nothing and directly exit the process_read_or_write. If there
  * no available threads, the work of choosing random thread will be
  * finished in the application thread end. If there are other available
  * threads, the whole application can still continuously run. 
  */
void ParallelVerifierMl::HandleNoRace(thread_t curr_thd_id)
{
	// ScopedLock lock(internal_lock_);
	// if(avail_thd_set_.empty() && 
	// 	ChooseRandomThreadAfterAllUnavailable()==curr_thd_id) {
	// 	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
	// 		thd_uid_map_[curr_thd_id]);
	// 	tls->status=AVAILABLE;
	// }
}

Verifier::VERIFY_RESULT ParallelVerifierMl::WaitVerification(address_t start_addr,
	address_t end_addr,PStmt *first_pstmt,PStmt *second_pstmt,Inst *inst,
	thread_t curr_thd_id,RaceEventType type,std::map<thread_t,bool> &pp_thd_map)
{
	PostponeThreadSet tmp_pp_thds;
	bool flag=false;
	VERIFY_RESULT res=NONE_SHARED;

	ThreadLocalStore *curr_tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	MAP_KEY_NOTFOUND_NEW(curr_tls->pstmt_metas_map,second_pstmt,MetaSet);
	// timestamp_t curr_thd_clk=curr_tls->vc.GetClock(curr_thd_id);
	SyncObject *sync_obj=GetVerifyRequest()->sync_obj;
	DEBUG_ASSERT(sync_obj);
	timestamp_t curr_thd_clk=sync_obj->vc.GetClock(curr_thd_id);
	// INFO_FMT_PRINT("===========wait verification start,addr:[%lx],thd_id:[%lx],first_pstmt:[%d]"
	// 		"============\n",start_addr,curr_thd_id,first_pstmt->line_);
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		Meta *meta=GetMeta(iaddr);
		if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst)) {
			res=REDUDANT;
			continue;
		}
		//traverse all thread local store
		for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
			thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
			ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
				iter->second);
			//different thread and the thread contains the accesses to the first_pstmt
			if(iter->first==curr_thd_id || tls->pstmt_metas_map.find(first_pstmt)==
				tls->pstmt_metas_map.end())
				continue;
			MetaSet *first_metas=tls->pstmt_metas_map[first_pstmt];
			DEBUG_ASSERT(first_metas);
			//pstmt accesss the same meta
			if(first_metas->find(meta)!=first_metas->end()) {
				//current thread is postponed and postponed metas contain current meta
				if(tls->status==BEING_POSTPONED && 
					tls->pp_metas.find(meta)!=tls->pp_metas.end()) {
					//traverse current thread's last snapshot
					MetaSnapshot *meta_ss=meta->LastestMetaSnapshot(iter->first);
					DEBUG_ASSERT(meta_ss);
					//filter irrelevant postponed thread or raced inst pair
					if(meta_ss->pstmt!=first_pstmt)
						continue; 
					if(meta->RacedInstPair(meta_ss->inst,inst)) {
						res=REDUDANT;
						continue ;
					}
					if(type==RACE_EVENT_WRITE) {
						flag=true;
						res=RACY;
						tmp_pp_thds.insert(iter->first);
						// write to write race
						if(meta_ss->type==RACE_EVENT_WRITE)
							PrintDebugRaceInfo(meta,WRITETOWRITE,iter->first,
								meta_ss->inst,curr_thd_id,inst);
						// read to write race
						else
							PrintDebugRaceInfo(meta,READTOWRITE,iter->first,
								meta_ss->inst,curr_thd_id,inst);
						// add race to race db
						ReportRace(meta,iter->first,meta_ss->inst,meta_ss->type,
							curr_thd_id,inst,type);
						meta->AddRacedInstPair(meta_ss->inst,inst);
					}
					else if(type==RACE_EVENT_READ) {
						if(meta_ss->type==RACE_EVENT_WRITE) {
							flag=true;
							res=RACY;
							tmp_pp_thds.insert(iter->first);
							PrintDebugRaceInfo(meta,WRITETOREAD,iter->first,
								meta_ss->inst,curr_thd_id,inst);
							//add race to race db
							ReportRace(meta,iter->first,meta_ss->inst,
								meta_ss->type,curr_thd_id,inst,type);
							meta->AddRacedInstPair(meta_ss->inst,inst);
						}
						else
							res=SHARED_READ;
					}// else if(type==RACE_EVENT_READ)
				}// if(tls->pp_metas.find(meta)!=tls->pp_metas.end())
				/* The postponed thread is waked up asynchronized and it's postponed
				   status is invalid. */
				else
					ClearWhenVerifyRequestInvalid(tls);
				// Send the historical detection request
				if(prl_vrf_num_>0) {
					DistributeHtyDtcRequest(meta,inst,curr_thd_id,type,second_pstmt);
				}
			}// if(first_metas->find(meta)!=first_metas->end())
			/* Parter pstmt does not access the same shared memory location with the
  			   current pstmt. */
			else
				res=NONE_SHARED;
		}// traverse all thread local store
	}

	InternalLock();
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		Meta *meta=GetMeta(iaddr);
		// Add current thread's meta snapshot
	 	AddMetaSnapshot(meta,curr_thd_id,curr_thd_clk,type,inst,second_pstmt);
		curr_tls->pp_metas.insert(meta);
		/* Add the meta into hy_metas in advance. If current thread is still postponed,
		   then the hy_metas will not be used. Only the postponed status is invalid,
		   hy_metas will be used for processing historical detection. */
		curr_tls->hy_metas.insert(meta);
		curr_tls->pstmt_metas_map[second_pstmt]->insert(meta);
	}

	// INFO_FMT_PRINT("======wait verification end========\n");
	// Find the verification race
	if(flag) {
		prace_db_->RemoveRelationMapping(first_pstmt,second_pstmt);
		//first pstmt has been fully verified
		if(prace_db_->HasFullyVerified(first_pstmt)) {
			ClearFullyVerifierPStmtMetas(first_pstmt);
			for(PostponeThreadSet::iterator iter=tmp_pp_thds.begin();
				iter!=tmp_pp_thds.end();iter++) {
				pp_thd_map[*iter]=false;
			}
		//first pstmt has not been fully verified
		} else {
			for(PostponeThreadSet::iterator iter=tmp_pp_thds.begin();
				iter!=tmp_pp_thds.end();iter++) {
				pp_thd_map[*iter]=true;
			}
		}
		if(prace_db_->HasFullyVerified(second_pstmt)) {
			//second pstmt has been fully verified
			ClearFullyVerifierPStmtMetas(second_pstmt);
			pp_thd_map[curr_thd_id]=false;
		}
		
	}
	InternalUnlock();
	return res;
}

ParallelVerifierMl::VerifyRequest* ParallelVerifierMl::GetVerifyRequest() 
{
	ScopedLock lock(internal_lock_);
	if(vrf_req_que_.empty())
		return NULL;
	VerifyRequest *req=vrf_req_que_.front();
	return req;
}

void ParallelVerifierMl::PopVerifyRequest() 
{
	ScopedLock lock(internal_lock_);
	if(!vrf_req_que_.empty())
		vrf_req_que_.pop();
}

void ParallelVerifierMl::DistributeEndOfSyncObj()
{
	SyncObject *sync_obj=GetVerifyRequest()->sync_obj;
	// Parallel history detection threads
	if(prl_vrf_num_>0) {
		// Update the request count
		sync_obj->cnt=htydtc_reqque_table_.size();
		// Sent the request of the end of sync obj
		for(HtyDtcReqQueueTable::iterator iter=htydtc_reqque_table_.begin();
			iter!=htydtc_reqque_table_.end();iter++) {
			HtyDtcRequestQueue *req_que=iter->second;
			req_que->push(new HtyDtcRequest(sync_obj));
		}
	}
	// Only contains a verification thread 
	else if(prl_vrf_num_<0) {
		delete sync_obj;
	}
}

void ParallelVerifierMl::DistributeHtyDtcRequest(Meta *meta,Inst *inst,
	thread_t curr_thd_id,RaceEventType type,PStmt *pstmt)
{
	HtyDtcRequestQueue *req_que=NULL;
	SyncObject *sync_obj=GetVerifyRequest()->sync_obj;
	DEBUG_ASSERT(sync_obj);
	int index=(meta->addr/unit_size_)%prl_vrf_num_;
	HtyDtcReqQueueTable::iterator iter=htydtc_reqque_table_.begin();
	std::advance(iter,index);
	req_que=iter->second;
	//create and enque the historical detection request
	req_que->push(new HtyDtcRequest(meta,pstmt,inst,curr_thd_id,type,sync_obj));
	// INFO_FMT_PRINT("===========push history detection request:[%s]==============\n",
	// 	inst->ToString().c_str());
}

RaceType ParallelVerifierMl::HistoryRace(MetaSnapshot *meta_ss,timestamp_t thd_clk,
	LockSet &rd_ls,LockSet &wr_ls,RaceEventType curr_type)
{
	MlMetaSnapshot *mlmeta_ss=dynamic_cast<MlMetaSnapshot*>(meta_ss);	
	LockSet *curr_rdls=&rd_ls;
	LockSet *curr_wrls=&wr_ls;
	if(curr_type==RACE_EVENT_WRITE && mlmeta_ss->thd_clk>thd_clk) {
		if(mlmeta_ss->type==RACE_EVENT_WRITE && 
			mlmeta_ss->wr_ls.Disjoint(curr_wrls)) {
			return WRITETOWRITE;
		}
		if(mlmeta_ss->type==RACE_EVENT_READ &&
			mlmeta_ss->wr_ls.Disjoint(curr_wrls) && 
			mlmeta_ss->rd_ls.Disjoint(curr_wrls) &&
			mlmeta_ss->wr_ls.Disjoint(curr_rdls)) {
			return READTOWRITE;
		}
	}
	else if(curr_type==RACE_EVENT_READ && mlmeta_ss->thd_clk>thd_clk) {
		if(mlmeta_ss->type==RACE_EVENT_WRITE && 
			mlmeta_ss->wr_ls.Disjoint(curr_wrls) && 
			mlmeta_ss->wr_ls.Disjoint(curr_rdls) &&
			mlmeta_ss->rd_ls.Disjoint(curr_wrls)) {
			return WRITETOREAD;
		} 
	}

	return NONE;
}

void ParallelVerifierMl::HistoryDetection(Meta *meta,PStmt *curr_pstmt,
	Inst *inst,thread_t curr_thd_id,RaceEventType type,SyncObject *sync_obj)
{
	VectorClock &vc=sync_obj->vc;
	LockSet &rd_ls=sync_obj->rd_ls;
	LockSet &wr_ls=sync_obj->wr_ls;
	//traverse all thread local store
	for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
		thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
		
		ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
			iter->second);
		
		// INFO_FMT_PRINT("==========history  other thread:[%lx],clk:[%ld]=========\n",
		// 		iter->first,vc.GetClock(iter->first));
		//different thread and current meta in the thread historical accessed meta set
		if(iter->first==curr_thd_id || tls->hy_metas.find(meta)==tls->hy_metas.end())
			continue ;
		if(meta->meta_ss_map.find(iter->first)==meta->meta_ss_map.end())
			continue;
		InternalLock();
		MetaSnapshotDeque *meta_ss_deq=meta->meta_ss_map[iter->first];
		for(MetaSnapshotDeque::iterator iiter=meta_ss_deq->begin();
			iiter!=meta_ss_deq->end();iiter++) {
			MetaSnapshot *meta_ss=*iiter;
			if(meta->RacedInstPair(meta_ss->inst,inst))
				continue ;
			//report history race and add to race db
			//copy the current thread clock and lockset information
			RaceType race_type=HistoryRace(meta_ss,vc.GetClock(iter->first),
				rd_ls,wr_ls,type);
			if(race_type==WRITETOWRITE || race_type==READTOWRITE ||
				race_type==WRITETOREAD) {
				PrintDebugRaceInfo(meta,race_type,iter->first,meta_ss->inst,
					curr_thd_id,inst);
				ReportRace(meta,iter->first,meta_ss->inst,meta_ss->type,
					curr_thd_id,inst,type);
				meta->AddRacedInstPair(meta_ss->inst,inst);
				
				//remove the raced pstmt pair mapping
				prace_db_->RemoveRelationMapping(meta_ss->pstmt,curr_pstmt);
				if(prace_db_->HasFullyVerified(meta_ss->pstmt))
					ClearFullyVerifierPStmtMetas(meta_ss->pstmt);
				if(prace_db_->HasFullyVerified(curr_pstmt))
					ClearFullyVerifierPStmtMetas(curr_pstmt);
			}
		}// traverse thread historical snapshot
		InternalUnlock();
	}
}

/**
  * This function is only invoked in the function 'postponethread'.
  * Need the outer synchronization.
  */
void ParallelVerifierMl::ClearPostponedThreadMetas(thread_t curr_thd_id) 
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	tls->status=AFTER_POSTPONED;
	if(!tls->pp_metas.empty())
		std::copy(tls->pp_metas.begin(),tls->pp_metas.end(),
			inserter(tls->hy_metas,tls->hy_metas.end()));
	// INFO_FMT_PRINT("==========clear postponed thread:[%lx] hy_metas size:[%ld]========\n",
	// 	curr_thd_id,tls->hy_metas.size());
}

//
void ParallelVerifierMl::ClearWhenVerifyRequestInvalid(ThreadLocalStore *tls)
{
	ScopedLock lock(internal_lock_);
	if(tls->status==AFTER_POSTPONED && !tls->pp_metas.empty()) {
		std::copy(tls->pp_metas.begin(),tls->pp_metas.end(),
			inserter(tls->hy_metas,tls->hy_metas.end()));
		tls->status=AVAILABLE;
	}
}

void ParallelVerifierMl::ClearFullyVerifierPStmtMetas(PStmt *pstmt)
{
	for(std::tr1::unordered_map<thread_t,uint32>::iterator iter=
		thd_uid_map_.begin();iter!=thd_uid_map_.end();iter++) {
		ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
			iter->second);
		if(tls->pstmt_metas_map.find(pstmt)!=tls->pstmt_metas_map.end()) {
			delete tls->pstmt_metas_map[pstmt];
			tls->pstmt_metas_map.erase(pstmt);
		}
	}
}

void ParallelVerifierMl::PushVerifyRequest(VerifyRequest *req,ThreadLocalStore *tls)
{
	SyncObject *obj=new SyncObject;
	ScopedLock lock(internal_lock_);
	obj->vc=tls->vc;
	obj->rd_ls=tls->rd_ls;
	obj->wr_ls=tls->wr_ls;
	req->sync_obj=obj;
	vrf_req_que_.push(req);
}

void ParallelVerifierMl::ProcessPostMutexLock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//set the vector clock
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	//set the owner
	mutex_meta->SetOwner(curr_thd_id);
	//if current not blocked, blk_thd_set_ and avail_thd_set_
	//result will not be changed
	UnblockThread(curr_thd_id);
	//update the lockset
	tls->wr_ls.Add(mutex_meta->addr);
}

void ParallelVerifierMl::ProcessPreMutexUnlock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	LockSet *curr_wrls=&tls->wr_ls;
	DEBUG_ASSERT(curr_wrls && curr_wrls->Exist(mutex_meta->addr));
	curr_wrls->Remove(mutex_meta->addr);
}

void ParallelVerifierMl::ProcessPostRwlockRdlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	UnblockThread(curr_thd_id);
	rwlock_meta->AddRdlockOwner(curr_thd_id);
	//update the lockset
	tls->rd_ls.Add(rwlock_meta->addr);
}

void ParallelVerifierMl::ProcessPostRwlockWrlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	UnblockThread(curr_thd_id);
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	rwlock_meta->SetWrlockOwner(curr_thd_id);
	//update the lockset
	tls->wr_ls.Add(rwlock_meta->addr);
}

void ParallelVerifierMl::ProcessPreRwlockUnlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	tls->wr_ls.Remove(rwlock_meta->addr);
	tls->rd_ls.Remove(rwlock_meta->addr);
}

void ParallelVerifierMl::ProcessSignal(thread_t curr_thd_id,CondMeta *cond_meta)
{
	ThreadLocalStore *tls=(ThreadLocalStore *)GetThreadData(tls_key_,
		thd_uid_map_[curr_thd_id]);
	//merge the known waiters' vc
	for(CondMeta::ThreadVectorClockMap::iterator iter=
		cond_meta->wait_table.begin();iter!=cond_meta->wait_table.end();
		iter++)
		tls->vc.Join(&iter->second);
	for(CondMeta::ThreadVectorClockMap::iterator iter=
		cond_meta->wait_table.begin();
		iter!=cond_meta->wait_table.end();iter++)
		cond_meta->signal_table[iter->first]=tls->vc;
	tls->vc.Increment(curr_thd_id);
}

} //namespace race