
#include <unistd.h>
#include "race/verifier.h"
#include "core/log.h"
#include "core/pin_util.h"

namespace race
{

Verifier::Verifier():internal_lock_(NULL),verify_lock_(NULL),prace_db_(NULL),
	filter_(NULL),unit_size_(4)
	{}

Verifier::~Verifier() 
{
	delete internal_lock_;
	delete verify_lock_;
	delete filter_;

	//some non-raced metas will be kept

	for(PStmtMetasMap::iterator iter=pstmt_metas_map_.begin();
		iter!=pstmt_metas_map_.end();iter++) {
		if(iter->second)
			delete iter->second;
	}
	for(ThreadMetasMap::iterator iter=thd_metas_map_.begin();
		iter!=thd_metas_map_.end();iter++) {
		if(iter->second)
			delete iter->second;
	}
}

bool Verifier::Enabled()
{
	return knob_->ValueBool("race_verify");
}

void Verifier::Register()
{
	knob_->RegisterBool("race_verify","whether enable the race verify","0");
	knob_->RegisterInt("unit_size_","the mornitoring granularity in bytes","4");
}

void Verifier::Setup(Mutex *internal_lock,Mutex *verify_lock,PRaceDB *prace_db)
{
	internal_lock_=internal_lock;
	verify_lock_=verify_lock;
	prace_db_=prace_db;
	unit_size_=knob_->ValueInt("unit_size_");
	filter_=new RegionFilter(internal_lock_->Clone());

	desc_.SetHookBeforeMem();
	desc_.SetHookPthreadFunc();
	desc_.SetHookMallocFunc();
	desc_.SetHookAtomicInst();
}

void Verifier::ImageLoad(Image *image,address_t low_addr,address_t high_addr,
	address_t data_start,size_t data_size,address_t bss_start,size_t bss_size)
{
	DEBUG_ASSERT(low_addr && high_addr && high_addr>low_addr);
	if(data_start) {
		DEBUG_ASSERT(data_size);
		AllocAddrRegion(data_start,data_size);
	}
	if(bss_start) {
		DEBUG_ASSERT(bss_size);
		AllocAddrRegion(bss_start,bss_size);
	}
}

void Verifier::ImageUnload(Image *image,address_t low_addr,address_t high_addr,
	address_t data_start,size_t data_size,address_t bss_start,size_t bss_size)
{
	DEBUG_ASSERT(low_addr);
	if(data_start)
		FreeAddrRegion(data_start);
	if(bss_start)
		FreeAddrRegion(bss_start);
}

void Verifier::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	// PinSemaphore *pin_sema=new PinSemaphore;
	SysSemaphore *sys_sema=new SysSemaphore(0);
	ScopedLock lock(internal_lock_);
	if(thd_smp_map_.find(curr_thd_id)==thd_smp_map_.end()) {
		// thd_smp_map_[curr_thd_id]=pin_sema;
		// thd_smp_map_[curr_thd_id]->Init();
		thd_smp_map_[curr_thd_id]=sys_sema;
	}
	//all threads are available at the beginning
	avail_thd_set_.insert(curr_thd_id);
}

void Verifier::ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk)
{
INFO_FMT_PRINT("=============postpone set size:[%ld]==============\n",pp_thd_set_.size());
	ScopedLock lock(internal_lock_);
	//
	if(thd_metas_map_.find(curr_thd_id)!=thd_metas_map_.end() && 
		thd_metas_map_[curr_thd_id]!=NULL)
		delete thd_metas_map_[curr_thd_id];
	thd_metas_map_.erase(curr_thd_id);
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

void Verifier::BeforePthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	BlockThread(curr_thd_id);
	//current thread is blocked
	if(avail_thd_set_.empty()) 
		ChooseRandomThreadAfterAllUnavailable();
	INFO_FMT_PRINT("================before pthread join avail_thd_set_.size:[%ld]\n",avail_thd_set_.size());
}

void Verifier::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	UnblockThread(curr_thd_id);
}

void Verifier::AfterMalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, size_t size, address_t addr)
{
	AllocAddrRegion(addr,size);
}

void Verifier::AfterCalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, size_t nmemb, size_t size,address_t addr)
{
	AllocAddrRegion(addr,size *nmemb);
}

void Verifier::BeforeRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t ori_addr, size_t size)
{
	FreeAddrRegion(ori_addr);
}
  
void Verifier::AfterRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t ori_addr, size_t size,address_t new_addr)
{
	AllocAddrRegion(new_addr,size);
}

void Verifier::BeforeFree(thread_t curr_thd_id, timestamp_t curr_thd_clk,
    Inst *inst, address_t addr)
{
	FreeAddrRegion(addr);
}

void Verifier::BeforeMemRead(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,size_t size)
{
	ChooseRandomThreadBeforeExecute(addr,curr_thd_id);
	ProcessReadOrWrite(curr_thd_id,inst,addr,size,READ);
}

void Verifier::BeforeMemWrite(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,size_t size)
{
	ChooseRandomThreadBeforeExecute(addr,curr_thd_id);
	ProcessReadOrWrite(curr_thd_id,inst,addr,size,WRITE);
}

void Verifier::BeforePthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	MAP_KEY_NOTFOUND_NEW(mutex_meta_table_,addr,MutexMeta);
	MutexMeta *mutex_meta=mutex_meta_table_[addr];

	thread_t thd_id=mutex_meta->GetOwner();
INFO_FMT_PRINT("================mutex lock owner:[%lx]\n",
		thd_id);
	//other thread has acquire the lock
	BlockThread(curr_thd_id);
	if(thd_id!=0) {	
		if(avail_thd_set_.empty() && 
			pp_thd_set_.find(thd_id)!=pp_thd_set_.end()) {
			WakeUpPostponeThread(thd_id);
		}
		// else do nothing blocked
	}
	INFO_FMT_PRINT("================before thread mutex lock avail_thd_set_ size:[%ld]\n",
		avail_thd_set_.size());
}
	
void Verifier::AfterPthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	//set the owner
	ScopedLock lock(internal_lock_);
	MutexMeta *mutex_meta=mutex_meta_table_[addr];
	mutex_meta->SetOwner(curr_thd_id);
	//if current not blocked, blk_thd_set_ and avail_thd_set_
	//result will not be changed
	UnblockThread(curr_thd_id);
}

void Verifier::BeforePthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{

}

void Verifier::AfterPthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	//clear the owner
	ScopedLock lock(internal_lock_);
	MutexMeta *mutex_meta=mutex_meta_table_[addr];
	mutex_meta->SetOwner(0);
}

void Verifier::BeforePthreadMutexTryLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{

}

void Verifier::AfterPthreadMutexTryLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,int retVal)
{

}

/**
 * multi-threaded region
 */
void Verifier::ChooseRandomThreadBeforeExecute(address_t addr,thread_t curr_thd_id)
{
	if(FilterAccess(addr))	
		return ;
	VerifyLock();
	//generate random thread id
	if(!avail_thd_set_.empty()) {
		while(RandomThread(avail_thd_set_)!=curr_thd_id) {
			VerifyUnlock();
			Sleep(1000);
		}
	}
}

/**
 * single-threaded region
 */
void Verifier::ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
	size_t size,bool is_read)
{
INFO_FMT_PRINT("========process read or write,curr_thd_id:[%lx]=======\n",curr_thd_id);
	//get the potential statement
	std::string file_name=inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	PStmt *pstmt=prace_db_->GetPStmt(file_name,line);
	DEBUG_ASSERT(pstmt);

	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);

	//first pstmt set of the pstmt pairs
	PStmtSet first_pstmts;
	//traverse the handled potential statements
	for(PStmtMetasMap::iterator iter=pstmt_metas_map_.begin();
		iter!=pstmt_metas_map_.end();iter++) {
		//find all matched potential statements
		if(prace_db_->SecondPotentialStatement(iter->first,pstmt)) {
			first_pstmts.insert(iter->first);
		}
	}

	//first accessed stmt of the potentail stmt pair,needed to be postponed 
	if(first_pstmts.empty()) {
		//add metas to pstmt accessed meta set
		MetaSet *metas=new MetaSet;
		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			Meta *meta=GetMeta(iaddr);
			if(is_read==READ) meta->AddReader();
			else meta->AddWriter();

			DEBUG_ASSERT(meta);
			metas->insert(meta);
		}
		//accumulate the metas into pstmt corresponding metas
		MAP_KEY_NOTFOUND_NEW(pstmt_metas_map_,pstmt,MetaSet);
		// if(pstmt_metas_map_.find(pstmt)==pstmt_metas_map_.end() ||
		// 	pstmt_metas_map_[pstmt]==NULL)
		// 	pstmt_metas_map_[pstmt]=new MetaSet;
		for(MetaSet::iterator iter=metas->begin();iter!=metas->end();iter++)
			pstmt_metas_map_[pstmt]->insert(*iter);		
		thd_metas_map_[curr_thd_id]=metas;

		//postpone current thread
		PostponeThread(curr_thd_id);
	} else {
		//accumulate postponed threads
		PostponeThreadSet pp_thds;
		for(PStmtSet::iterator iter=first_pstmts.begin();iter!=first_pstmts.end();
			iter++) {
			PStmt *first_pstmt=*iter;
			MetaSet raced_metas;
			RacedMeta(first_pstmt,start_addr,end_addr,pstmt,inst,curr_thd_id,
				is_read,raced_metas);
			//found raced metas
			if(!raced_metas.empty()) {
				//for each raced meta
				for(MetaSet::iterator iiter=raced_metas.begin();iiter!=raced_metas.end();
					iiter++) {
					//traverse postponed threads
					for(ThreadMetasMap::iterator iiiter=thd_metas_map_.begin();
						iiiter!=thd_metas_map_.end();iiiter++) {
						//must check thread whether in the pp_thd_set_
						if(pp_thd_set_.find(iiiter->first)!=pp_thd_set_.end() && 
							iiiter->second!=NULL && 
							iiiter->second->find(*iiter)!=iiiter->second->end()) {
							pp_thds.insert(iiiter->first);
							//clear the entire metas of the thread from the pstmt
							//corresponding metas
							MetaSet *metas=thd_metas_map_[iiiter->first];
							ClearPStmtCorrespondingMetas(first_pstmt,metas);
						}
					}
					//decrease writer or reader reference
					(*iiter)->CutWriter();
					(*iiter)->CutReader();
				}
				//remove the raced pstmt mapping
				prace_db_->RemoveRelationMapping(first_pstmt,pstmt);
			}
		}

		if(!pp_thds.empty()) {
			//clear current thread accessed metas and pstmt corresponding metas
			MetaSet *metas=thd_metas_map_[curr_thd_id];
			DEBUG_ASSERT(metas);
			ClearPStmtCorrespondingMetas(pstmt,metas);
			HandleRace(&pp_thds,curr_thd_id);
		// have no shared memory
		}else
			HandleNoRace(curr_thd_id);
	}
INFO_FMT_PRINT("=========process read or write end:[%lx]=========\n",curr_thd_id);
}

void Verifier::FreeAddrRegion(address_t addr)
{
	ScopedLock lock(internal_lock_);
	if(!addr) return ;
	size_t size=filter_->RemoveRegion(addr,false);

	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);

	//for memory
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr +=unit_size_) {
		Meta::Table::iterator it=meta_table_.find(iaddr);
		if(it!=meta_table_.end()) {
			delete it->second;
			meta_table_.erase(it);
		}
	}
}

void Verifier::AllocAddrRegion(address_t addr,size_t size)
{
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(addr && size);
	filter_->AddRegion(addr,size,false);
}

Verifier::Meta* Verifier::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new Meta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

//
void Verifier::HandleNoRace(thread_t curr_thd_id)
{
INFO_PRINT("=================handle no race===================\n");
	PostponeThread(curr_thd_id);
}
//
void Verifier::HandleRace(PostponeThreadSet *pp_thds,thread_t curr_thd_id)
{
INFO_PRINT("=================handle race===================\n");
	//clear raced info whenever
	for(PostponeThreadSet::iterator iter=pp_thds->begin();iter!=pp_thds->end();
		iter++) {
INFO_FMT_PRINT("+++++++++++++++++++pp_thd_id:[%lx]+++++++++++++++++\n",*iter);
		//clear raced metas
		delete thd_metas_map_[*iter];
		thd_metas_map_[*iter]=NULL;
	}

	delete thd_metas_map_[curr_thd_id];
	thd_metas_map_[curr_thd_id]=NULL;

	//wake up postpone thread set,postpone current thread
	if(!RandomBool()) {
		WakeUpPostponeThreadSet(pp_thds);
		PostponeThread(curr_thd_id);
	}
	//must free the lock if current thread execute continuously
	else
		VerifyUnlock();
}

/**
 * must free lock at each branch of the procedure
 */
void Verifier::PostponeThread(thread_t curr_thd_id)
{
INFO_FMT_PRINT("=================postpone thread:[%lx]===================\n",curr_thd_id);

	//correspond to before_mutex_lock
	InternalLock();
INFO_FMT_PRINT("=================avail_thd_set_ size:[%ld]===================\n",avail_thd_set_.size());
	//current thread is the only available thread, others may be blocked by some sync
	//operations, we should not postpone it

	if(avail_thd_set_.size()==1 && pp_thd_set_.empty()) {
		InternalUnlock();
		VerifyUnlock();		
		return ;
	}

	pp_thd_set_.insert(curr_thd_id);
	avail_thd_set_.erase(curr_thd_id);
	//must choose one thread execute continuously if all threads are unavailable
	if(avail_thd_set_.empty())
		ChooseRandomThreadAfterAllUnavailable();

	InternalUnlock();
	VerifyUnlock();
	//semahore wait
	thd_smp_map_[curr_thd_id]->Wait();
INFO_FMT_PRINT("=================after wait:[%lx]===================\n",curr_thd_id);
}

void Verifier::ChooseRandomThreadAfterAllUnavailable()
{
	if(pp_thd_set_.empty())
		return ;
	thread_t thd_id=RandomThread(pp_thd_set_);
INFO_FMT_PRINT("=================needed to wakeup:[%lx]===================\n",thd_id);
	DEBUG_ASSERT(thd_smp_map_[thd_id]);
	// if(thd_smp_map_[thd_id]->IsWaiting())
	// 	thd_smp_map_[thd_id]->Post();
	WakeUpPostponeThread(thd_id);
}

inline void Verifier::WakeUpPostponeThread(thread_t thd_id)
{
	thd_smp_map_[thd_id]->Post();
	pp_thd_set_.erase(thd_id);
	avail_thd_set_.insert(thd_id);
}

void Verifier::WakeUpPostponeThreadSet(PostponeThreadSet *pp_thds)
{
INFO_FMT_PRINT("=================wakeup pp_thds size:[%ld]===================\n",pp_thds->size());
	DEBUG_ASSERT(pp_thds);
	for(PostponeThreadSet::iterator iter=pp_thds->begin();iter!=pp_thds->end();
		iter++) {
		// if(thd_smp_map_[*iter]->IsWaiting())
		// 	thd_smp_map_[*iter]->Post();
		WakeUpPostponeThread(*iter);
		//clear postponed threads' corresponding metas
		delete thd_metas_map_[*iter];
		thd_metas_map_.erase(*iter);
	}
}

inline 
void Verifier::ClearPStmtCorrespondingMetas(PStmt *pstmt,MetaSet *metas)
{
	DEBUG_ASSERT(pstmt && metas);
	if(pstmt_metas_map_.find(pstmt)==pstmt_metas_map_.end() ||
		pstmt_metas_map_[pstmt]==NULL)
		return ;
	MetaSet *pstmt_metas=pstmt_metas_map_[pstmt];
	for(MetaSet::iterator iter=metas->begin();iter!=metas->end();iter++) {
		if(pstmt_metas->find(*iter)!=pstmt_metas->end())
			pstmt_metas->erase(*iter);
	}
	if(pstmt_metas->empty()) {
		delete pstmt_metas;
		pstmt_metas_map_[pstmt]=NULL;
	}
}

void Verifier::RacedMeta(PStmt *first_pstmt,address_t start_addr,address_t end_addr,
	PStmt *second_pstmt,Inst *inst,thread_t curr_thd_id,bool is_read,
	MetaSet &raced_metas)
{
	if(pstmt_metas_map_.find(first_pstmt)==pstmt_metas_map_.end() || 
		pstmt_metas_map_[first_pstmt]==NULL)
		return ;
	MetaSet *first_metas=pstmt_metas_map_[first_pstmt];
	// if(pstmt_metas_map_.find(second_pstmt)==pstmt_metas_map_.end() ||
	// 	pstmt_metas_map_[second_pstmt]==NULL)
	// 	pstmt_metas_map_[second_pstmt]=new MetaSet;
	MAP_KEY_NOTFOUND_NEW(pstmt_metas_map_,second_pstmt,MetaSet);
	MetaSet *second_metas=pstmt_metas_map_[second_pstmt];
	//current thread acccessed metas
	MetaSet *metas=new MetaSet;
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		Meta *meta=GetMeta(iaddr);
		DEBUG_ASSERT(meta);
		//
		if(first_metas->find(meta)!=first_metas->end()) {
			//write access
			if(is_read==WRITE) {
				if(meta->GetWriters()>0 || meta->GetReaders()>0 ) {
					raced_metas.insert(meta);
					if(meta->GetWriters()>0 && meta->GetReaders()<=0)
						PrintDebugRaceInfo(meta,WRITETOWRITE,inst);
					else
						PrintDebugRaceInfo(meta,READTOWRITE,inst);
				}
			}
			//read access
			else {
				if(meta->GetWriters()>0) {
					raced_metas.insert(meta);
					PrintDebugRaceInfo(meta,WRITETOREAD,inst);
				}
			}
 		}
 		metas->insert(meta);
		second_metas->insert(meta);
	}
	//
	thd_metas_map_[curr_thd_id]=metas;

	//traverse again
	if(raced_metas.empty()) {
		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			Meta *meta=GetMeta(iaddr);
			if(is_read==WRITE) meta->AddWriter();
			else meta->AddReader();
		}
	}
	//if race exists,then we ignore the meta's writer or reader reference
	//because we will not decrease the reference for those metas not in raced metas
}

void Verifier::PrintDebugRaceInfo(Meta *meta,RaceType race_type,Inst *inst)
{
	const char *race_type_name;
	switch(race_type) {
		case WRITETOREAD:
			race_type_name="WAR";
			break;
		case WRITETOWRITE:
			race_type_name="WAW";
			break;
		case READTOWRITE:
			race_type_name="RAW";
			break;
		case WRITETOREAD_OR_READTOWRITE:
			race_type_name="RAW|WAR";
			break;
		default:
			break;
	}
	DEBUG_FMT_PRINT_SAFE("%s%s\n",SEPARATOR,SEPARATOR);
	DEBUG_FMT_PRINT_SAFE("%s race detected [T??]\n",race_type_name);
	DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",meta->addr);
	DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n",inst->ToString().c_str());
	DEBUG_FMT_PRINT_SAFE("%s%s\n",SEPARATOR,SEPARATOR);
}

}// namespace race