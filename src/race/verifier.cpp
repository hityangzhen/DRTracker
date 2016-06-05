
#include <unistd.h>
#include "race/verifier.h"
#include "core/log.h"
#include "core/pin_util.h"
#include <time.h>
#include <algorithm>
namespace race
{

void CallStack::PrintCallStack() 
{
	INFO_FMT_PRINT("==============bgn_idx:[%d],end_idx[%d]=============\n",bgn_idx_,end_idx_);
	if(bgn_idx_!=end_idx_) {
		if(bgn_idx_<end_idx_)
			for(int i=bgn_idx_;i<end_idx_;i++)
				INFO_FMT_PRINT("[CALLSTACK] %s\n",vec_[i]->c_str());
		else {
			for(unsigned int i=bgn_idx_;i<vec_.size();i++)
				INFO_FMT_PRINT("[CALLSTACK] %s\n",vec_[i]->c_str());
			for(int i=0;i<end_idx_;i++)
				INFO_FMT_PRINT("[CALLSTACK] %s\n",vec_[i]->c_str());	
		}
	}
}

//static initialize
size_t Verifier::ss_deq_len=0;
address_t Verifier::unit_size_=0;
// bool Verifier::history_race_analysis_=true;
Verifier::Verifier():internal_lock_(NULL),verify_lock_(NULL),prace_db_(NULL),
	filter_(NULL),flags_(0),mem_bug_(NULL) {}

Verifier::~Verifier() 
{
	delete internal_lock_;
	delete verify_lock_;
	delete filter_;
	delete mem_bug_;
	//clear vector clock
	for(ThreadVectorClockMap::iterator iter=thd_vc_map_.begin();
		iter!=thd_vc_map_.end();iter++) {
		delete iter->second;
	}
	//clear metas
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
	//clear sync metas
	for(MutexMeta::Table::iterator iter=mutex_meta_table_.begin();
		iter!=mutex_meta_table_.end();iter++) {
		if(iter->second)
			ProcessFree(iter->second);
	}
	for(RwlockMeta::Table::iterator iter=rwlock_meta_table_.begin();
		iter!=rwlock_meta_table_.end();iter++) {
		if(iter->second)
			ProcessFree(iter->second);
	}
	for(BarrierMeta::Table::iterator iter=barrier_meta_table_.begin();
		iter!=barrier_meta_table_.end();iter++) {
		if(iter->second)
			ProcessFree(iter->second);
	}
	for(CondMeta::Table::iterator iter=cond_meta_table_.begin();
		iter!=cond_meta_table_.end();iter++) {
		if(iter->second)
			ProcessFree(iter->second);
	}
	for(SemMeta::Table::iterator iter=sem_meta_table_.begin();
		iter!=sem_meta_table_.end();iter++) {
		if(iter->second)
			ProcessFree(iter->second);
	}
	for(ThreadCallStackTable::iterator iter=thd_callstack_table_.begin();
		iter!=thd_callstack_table_.end();iter++)
		delete iter->second;
}

bool Verifier::Enabled()
{
	return knob_->ValueBool("race_verify");
}

void Verifier::Register()
{
	knob_->RegisterBool("race_verify","whether enable the race verify","0");
	knob_->RegisterInt("unit_size_","the mornitoring granularity in bytes","4");
	knob_->RegisterInt("ss_deq_len","max length of the snapshot vector","10");
	knob_->RegisterBool("history_race_analysis","whether do the history race"
		" analysis","0");
	knob_->RegisterBool("harmful_race_analysis","whether do the harmful race"
		" analysis","0");
	knob_->RegisterStr("null_ptr_deref","file name of the static information"
		" about the null pointer dereference analysis","0");
	knob_->RegisterStr("dangling_ptr","file name of the static information"
		" about the dangling pointer analysis","0");
	knob_->RegisterStr("buffer_overflow","file name of the static information"
		" about the buffer index","0");
}

void Verifier::Setup(Mutex *internal_lock,Mutex *verify_lock,RaceDB *race_db,
	PRaceDB *prace_db)
{
	internal_lock_=internal_lock;
	verify_lock_=verify_lock;
	unit_size_=knob_->ValueInt("unit_size_");
	race_db_=race_db;
	prace_db_=prace_db;
	filter_=new RegionFilter(internal_lock_->Clone());

	desc_.SetHookBeforeMem();
	desc_.SetHookPthreadFunc();
	desc_.SetHookMallocFunc();
	desc_.SetHookAtomicInst();
	desc_.SetHookCallReturn();
	desc_.SetHookMainFunc();
	//max length of the snapshot vector
	ss_deq_len=knob_->ValueInt("ss_deq_len");
	if(ss_deq_len<=0)
		ss_deq_len=1;
	if(knob_->ValueBool("history_race_analysis"))
		// history_race_analysis_=false;
		SET_HISTORY_RACE_ANALYSIS(flags_);
	if(knob_->ValueBool("harmful_race_analysis")) {
		SET_HARMFUL_RACE_ANALYSIS(flags_);
		mem_bug_=new MemBug(knob_);
		//mem_bug_->Register();
		mem_bug_->Setup();
	}
}

void Verifier::ImageLoad(Image *image,address_t low_addr,address_t high_addr,
	address_t data_start,size_t data_size,address_t bss_start,size_t bss_size)
{
	DEBUG_ASSERT(low_addr && high_addr && high_addr>low_addr);
	if(data_start) {
		DEBUG_ASSERT(data_size);
		if(mem_bug_)
			mem_bug_->AddGlobalRegion(data_start,data_size);
		AllocAddrRegion(data_start,data_size);
	}
	if(bss_start) {
		DEBUG_ASSERT(bss_size);
		if(mem_bug_)
			mem_bug_->AddGlobalRegion(bss_start,bss_size);
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
	VectorClock *curr_vc=new VectorClock;
	CallStack *callstack=new CallStack;

	ScopedLock lock(internal_lock_);
	thd_callstack_table_[curr_thd_id]=callstack;
	//thread vector clock
	curr_vc->Increment(curr_thd_id);
	//not the main thread
	if(parent_thd_id!=INVALID_THD_ID) {
		VectorClock *parent_vc=thd_vc_map_[parent_thd_id];
		DEBUG_ASSERT(parent_vc);
		curr_vc->Join(parent_vc);
		parent_vc->Increment(parent_thd_id);
	}
	thd_vc_map_[curr_thd_id]=curr_vc;
	//thread semaphore
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
// INFO_FMT_PRINT("===========thread:[%lx] exit,postpone set size:[%ld]=============\n",
// 	curr_thd_id,pp_thd_set_.size());
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

void Verifier::BeforePthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	BlockThread(curr_thd_id);
	//current thread is blocked
	if(avail_thd_set_.empty()) 
		ChooseRandomThreadAfterAllUnavailable();
}

void Verifier::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	//thread vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	VectorClock *child_vc=thd_vc_map_[child_thd_id];
	curr_vc->Join(child_vc);
	curr_vc->Increment(curr_thd_id);

	UnblockThread(curr_thd_id);
}

void Verifier::AfterPthreadCreate(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  	Inst *inst,thread_t child_thd_id)
{ }

void Verifier::BeforeCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,std::string *funcname,address_t target)
{
	CallStack *callstack=thd_callstack_table_[curr_thd_id];
	if(callstack->Full())
		callstack->PopFront();
	callstack->Push(funcname);
}

void Verifier::BeforeReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,std::string *funcname,address_t target)
{
	CallStack *callstack=thd_callstack_table_[curr_thd_id];
	if(!callstack->Empty())
		callstack->PopBack();
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
	if(!ChooseRandomThreadBeforeProcess(addr,curr_thd_id))
		return;
	ProcessReadOrWrite(curr_thd_id,inst,addr,size,RACE_EVENT_READ);
}

void Verifier::BeforeMemWrite(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,size_t size)
{
	if(!ChooseRandomThreadBeforeProcess(addr,curr_thd_id))
		return ;
	ProcessReadOrWrite(curr_thd_id,inst,addr,size,RACE_EVENT_WRITE);
}

void Verifier::BeforePthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	MutexMeta *mutex_meta=GetMutexMeta(addr);
	ProcessPreMutexLock(curr_thd_id,mutex_meta);
}
	
void Verifier::AfterPthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	MutexMeta *mutex_meta=GetMutexMeta(addr);
	ProcessPostMutexLock(curr_thd_id,mutex_meta);
}

void Verifier::BeforePthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	//set the vector clock
	MutexMeta *mutex_meta=GetMutexMeta(addr);
	ProcessPreMutexUnlock(curr_thd_id,mutex_meta);
}

void Verifier::AfterPthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	//clear the owner
	MutexMeta *mutex_meta=GetMutexMeta(addr);
	ProcessPostMutexUnlock(curr_thd_id,mutex_meta);
}
// untested
void Verifier::BeforePthreadMutexTryLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BeforePthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}
// untested
void Verifier::AfterPthreadMutexTryLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val)
{
	if(ret_val==0)
		AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
	else
		UnblockThread(curr_thd_id);
}

void Verifier::BeforePthreadRwlockRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPreRwlockRdlock(curr_thd_id,rwlock_meta);
}

void Verifier::AfterPthreadRwlockRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPostRwlockRdlock(curr_thd_id,rwlock_meta);
}

void Verifier::BeforePthreadRwlockWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPreRwlockWrlock(curr_thd_id,rwlock_meta);
}

void Verifier::AfterPthreadRwlockWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPostRwlockWrlock(curr_thd_id,rwlock_meta);
}

void Verifier::BeforePthreadRwlockUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPreRwlockUnlock(curr_thd_id,rwlock_meta);
}

void Verifier::AfterPthreadRwlockUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	RwlockMeta *rwlock_meta=GetRwlockMeta(addr);
	ProcessPostRwlockUnlock(curr_thd_id,rwlock_meta);
}

void Verifier::BeforePthreadRwlockTryRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BeforePthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Verifier::AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val)
{
	if(ret_val==0)
		AfterPthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
	else
		UnblockThread(curr_thd_id);
}

void Verifier::BeforePthreadRwlockTryWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BeforePthreadRwlockWrlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Verifier::AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr,int ret_val)
{
	if(ret_val==0)
		AfterPthreadRwlockWrlock(curr_thd_id,curr_thd_clk,inst,addr);
	else
		UnblockThread(curr_thd_id);
}

void Verifier::AfterPthreadBarrierInit(thread_t curr_thd_id,
  	timestamp_t curr_thd_clk, Inst *inst,address_t addr, unsigned int count)
{
	ScopedLock lock(internal_lock_);
	BarrierMeta *barrier_meta=GetBarrierMeta(addr);
	barrier_meta->count=count;
}

void Verifier::BeforePthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	BarrierMeta *barrier_meta=GetBarrierMeta(addr);
	BlockThread(curr_thd_id);
	//set the vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	barrier_meta->vc.Join(curr_vc);
	barrier_meta->ref++;
	//
	if(barrier_meta->ref!=barrier_meta->count && avail_thd_set_.empty()) {
		// if(!pp_thd_set_.empty())
			ChooseRandomThreadAfterAllUnavailable();
		// else go through
	}
}
  	
void Verifier::AfterPthreadBarrierWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	BarrierMeta *barrier_meta=GetBarrierMeta(addr);
	UnblockThread(curr_thd_id);
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	curr_vc->Join(&barrier_meta->vc);
	curr_vc->Increment(curr_thd_id);
	if(--(barrier_meta->ref)==0)
		barrier_meta->vc.Clear();
}

void Verifier::BeforePthreadCondSignal(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	CondMeta *cond_meta=GetCondMeta(addr);
	ProcessSignal(curr_thd_id,cond_meta);
}

void Verifier::BeforePthreadCondBroadcast(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst, address_t addr)
{
	ScopedLock lock(internal_lock_);
	CondMeta *cond_meta=GetCondMeta(addr);
	ProcessSignal(curr_thd_id,cond_meta);
}

void Verifier::BeforePthreadCondWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	ScopedLock lock(internal_lock_);
	//handle unlock
	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
	ProcessPreMutexUnlock(curr_thd_id,mutex_meta);
	ProcessPostMutexUnlock(curr_thd_id,mutex_meta);
	//
	CondMeta *cond_meta=GetCondMeta(cond_addr);
	BlockThread(curr_thd_id);
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	cond_meta->wait_table[curr_thd_id]=*curr_vc;

	if(avail_thd_set_.empty()) {
		// if(!pp_thd_set_.empty()) 
			ChooseRandomThreadAfterAllUnavailable();
		// else go through
	}
}

void Verifier::AfterPthreadCondWait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr, 
    address_t mutex_addr)
{
	ScopedLock lock(internal_lock_);
	CondMeta *cond_meta=GetCondMeta(cond_addr);
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	//clear from the wait table
	CondMeta::ThreadVectorClockMap::iterator witer=
		cond_meta->wait_table.find(curr_thd_id);
	DEBUG_ASSERT(witer!=cond_meta->wait_table.end());
	cond_meta->wait_table.erase(witer);
	//clear the signal info
	CondMeta::ThreadVectorClockMap::iterator siter=
		cond_meta->signal_table.find(curr_thd_id);
	if(siter!=cond_meta->signal_table.end()) {
		curr_vc->Join(&siter->second);
		cond_meta->signal_table.erase(siter);
	}
	//
	UnblockThread(curr_thd_id);
	curr_vc->Increment(curr_thd_id);
	//handle lock
	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
	ProcessPreMutexLock(curr_thd_id,mutex_meta);
	ProcessPostMutexLock(curr_thd_id,mutex_meta);
}
//untested
void Verifier::BeforePthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}
//untested
void Verifier::AfterPthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void Verifier::AfterSemInit(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  	Inst *inst,address_t addr,unsigned int value)
{
	ScopedLock lock(internal_lock_);
	SemMeta *sem_meta=GetSemMeta(addr);
	sem_meta->count=value;
}

void Verifier::BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	SemMeta *sem_meta=GetSemMeta(addr);
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	sem_meta->vc.Join(curr_vc);
	//this step we move from the AfterSemPost to BeforeSemPost
	//and this will not affect the correctness
	curr_vc->Increment(curr_thd_id);
	sem_meta->count++;
}

void Verifier::BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	SemMeta *sem_meta=GetSemMeta(addr);
	BlockThread(curr_thd_id);
	//
	if(--(sem_meta->count)<0) {
		if(avail_thd_set_.empty()) {
			// if(!pp_thd_set_.empty())
				ChooseRandomThreadAfterAllUnavailable();
			// else go through
		}
	}
}

void Verifier::AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
     Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	SemMeta *sem_meta=GetSemMeta(addr);
	UnblockThread(curr_thd_id);
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	curr_vc->Join(&sem_meta->vc);
	curr_vc->Increment(curr_thd_id);
}

/**
 * multi-threaded region
 */
bool Verifier::ChooseRandomThreadBeforeProcess(address_t addr,thread_t curr_thd_id)
{
	if(FilterAccess(addr))	
		return false;
	//generate random thread id
	while(true) {
		VerifyLock();
		if(RandomThread(avail_thd_set_)==curr_thd_id)
			return true;
		VerifyUnlock();
		Sleep(50);
	}
}

/**
 * single-threaded region,when entering this function, current thread
 * has hold the verify_lock_.
 * each branch exit the function should free the lock
 */
void Verifier::ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
	size_t size,RaceEventType type)
{
// INFO_FMT_PRINT("=========process read or write start:[%lx]=========\n",curr_thd_id);
	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);
	//get the potential statement
	std::string file_name=inst->GetFileName();
	// size_t found=file_name.find_last_of("/");
	// file_name=file_name.substr(found+1);
	int line=inst->GetLine();	
	PStmt *pstmt=prace_db_->GetPStmt(file_name,line);
	//typically lock signal write is not raced with cond_wait read
	if(pstmt==NULL) {
		VerifyUnlock();
		return ;
	}
	// DEBUG_ASSERT(pstmt);
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
	timestamp_t curr_thd_clk=thd_vc_map_[curr_thd_id]->GetClock(curr_thd_id);
	//first accessed stmt of the potentail stmt pair,needed to be postponed 
	if(first_pstmts.empty()) {
		//indicate all the same meta snapshot
		bool flag=true;
		//accumulate the metas into pstmt corresponding metas
		MAP_KEY_NOTFOUND_NEW(pstmt_metas_map_,pstmt,MetaSet);
		MAP_KEY_NOTFOUND_NEW(thd_ppmetas_map_,curr_thd_id,MetaSet);

		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			Meta *meta=GetMeta(iaddr);
			DEBUG_ASSERT(meta);
			pstmt_metas_map_[pstmt]->insert(meta);
			// thd_metas_map_[curr_thd_id]->insert(meta);
			thd_ppmetas_map_[curr_thd_id]->insert(meta);
			//we assume that same inst and the same epoch in current threads'
			//recent snapshots means that the access is redudant 
			if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst))
				continue;
			//add snapshot
			AddMetaSnapshot(meta,curr_thd_id,curr_thd_clk,type,inst,pstmt);
			flag=false;
		}
		if(!flag) //postpone current thread
			PostponeThread(curr_thd_id);
		else {
			//recently accessed metas, current thread need not to be postponed
			delete thd_ppmetas_map_[curr_thd_id];
			thd_ppmetas_map_.erase(curr_thd_id); 
			VerifyUnlock();
			return ;
		}
	} else {
		bool redudant_flag=true;
		for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
			Meta *meta=GetMeta(iaddr);
			DEBUG_ASSERT(meta);
			if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst))
				continue;
			redudant_flag=false;
		}
		//accumulate postponed threads
		std::map<thread_t,uint8> pp_thd_map;
		VERIFY_RESULT res=NONE_SHARED;
		if(!redudant_flag) {
			// PostponeThreadSet pp_thds;
			for(PStmtSet::iterator iter=first_pstmts.begin();
				iter!=first_pstmts.end();iter++) {
				PStmt *first_pstmt=*iter;
				res=WaitVerificationAndHistoryDetection(first_pstmt,
					start_addr,end_addr,pstmt,inst,curr_thd_id,type,pp_thd_map);
			}
		}
		if(!pp_thd_map.empty()) {
			HandleRace(pp_thd_map,curr_thd_id);
		}else {
			//only choose one of the REDUDANT and NONE_SHARED 
			if(res==NONE_SHARED)
				KeepupThread(curr_thd_id);
			else
				HandleNoRace(curr_thd_id);
		}
	}
// INFO_FMT_PRINT("=========process read or write end:[%lx]=========\n",curr_thd_id);
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
		Meta::Table::iterator iter=meta_table_.find(iaddr);
		if(iter!=meta_table_.end()) {
			ProcessFree(iter->second);
			meta_table_.erase(iter);
		}
	}
}

void Verifier::AllocAddrRegion(address_t addr,size_t size)
{
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(addr && size);
	filter_->AddRegion(addr,size,false);
}

Verifier::Meta* Verifier::GetMeta(address_t addr)
{
	Meta::Table::iterator iter=meta_table_.find(addr);
	if(iter==meta_table_.end()) {
		Meta *meta=new Meta(addr);
		meta_table_[addr]=meta;
		return meta;
	}
	return iter->second;
}

Verifier::MutexMeta* Verifier::GetMutexMeta(address_t addr)
{
	MutexMeta::Table::iterator iter=mutex_meta_table_.find(addr);
	if(iter==mutex_meta_table_.end()) {
		MutexMeta *mutex_meta=new MutexMeta(addr);
		mutex_meta_table_[addr]=mutex_meta;
		return mutex_meta;
	}
	return iter->second;
}

Verifier::RwlockMeta* Verifier::GetRwlockMeta(address_t addr)
{
	RwlockMeta::Table::iterator iter=rwlock_meta_table_.find(addr);
	if(iter==rwlock_meta_table_.end()) {
		RwlockMeta *rwlock_meta=new RwlockMeta(addr);
		rwlock_meta_table_[addr]=rwlock_meta;
		return rwlock_meta;
	}
	return iter->second;
}

Verifier::BarrierMeta* Verifier::GetBarrierMeta(address_t addr)
{
	BarrierMeta::Table::iterator iter=barrier_meta_table_.find(addr);
	if(iter==barrier_meta_table_.end()) {
		BarrierMeta *barrier_meta=new BarrierMeta;
		barrier_meta_table_[addr]=barrier_meta;
		return barrier_meta;
	}
	return iter->second;
}

Verifier::CondMeta* Verifier::GetCondMeta(address_t addr)
{
	CondMeta::Table::iterator iter=cond_meta_table_.find(addr);
	if(iter==cond_meta_table_.end()) {
		CondMeta *cond_meta=new CondMeta;
		cond_meta_table_[addr]=cond_meta;
		return cond_meta;
	}
	return iter->second;
}

Verifier::SemMeta* Verifier::GetSemMeta(address_t addr)
{
	SemMeta::Table::iterator iter=sem_meta_table_.find(addr);
	if(iter==sem_meta_table_.end()) {
		SemMeta *sem_meta=new SemMeta;
		sem_meta_table_[addr]=sem_meta;
		return sem_meta;
	}
	return iter->second;
}

inline void Verifier::ProcessFree(Meta *meta)
{
	delete meta;
}

void Verifier::ProcessPreMutexLock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	thread_t thd_id=mutex_meta->GetOwner();
	//other thread has acquire the lock
	BlockThread(curr_thd_id);
	if(thd_id!=0) {	
		if(avail_thd_set_.empty() && 
			pp_thd_set_.find(thd_id)!=pp_thd_set_.end()) {
			WakeUpPostponeThread(thd_id);
		}
		// else do nothing blocked
	}
}

void Verifier::ProcessPostMutexLock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//set the vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	curr_vc->Join(&mutex_meta->vc);	
	//set the owner
	mutex_meta->SetOwner(curr_thd_id);
	//if current not blocked, blk_thd_set_ and avail_thd_set_
	//result will not be changed
	UnblockThread(curr_thd_id);
}

void Verifier::ProcessPreMutexUnlock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	//set the vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	mutex_meta->vc=*curr_vc;
	curr_vc->Increment(curr_thd_id);
}

void Verifier::ProcessPostMutexUnlock(thread_t curr_thd_id,
	MutexMeta *mutex_meta)
{
	mutex_meta->SetOwner(0);
}

void Verifier::ProcessPreRwlockRdlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	thread_t wrlock_thd_id=rwlock_meta->GetWrlockOwner();
	BlockThread(curr_thd_id);

	if(wrlock_thd_id!=0) {
		if(avail_thd_set_.empty() && 
			pp_thd_set_.find(wrlock_thd_id)!=pp_thd_set_.end()) {
			WakeUpPostponeThread(wrlock_thd_id);
		}
	}
}

void Verifier::ProcessPostRwlockRdlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	//set the vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	curr_vc->Join(&rwlock_meta->vc);

	UnblockThread(curr_thd_id);
	rwlock_meta->AddRdlockOwner(curr_thd_id);
	rwlock_meta->ref++;
}

void Verifier::ProcessPreRwlockWrlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	RwlockMeta::ThreadSet *rdlock_thd_set=rwlock_meta->GetRdlockOwners();
	BlockThread(curr_thd_id);
	//here we only consider the simple scene
	//there are no other 
	if(rwlock_meta->HasRdlockOwner()) {
		if(avail_thd_set_.empty()) {
			//we wakeup all readers
			for(RwlockMeta::ThreadSet::iterator iter=rdlock_thd_set->begin();
				iter!=rdlock_thd_set->end();iter++) {
				WakeUpPostponeThread(*iter);
			}
		}
	}
}
  	
void Verifier::ProcessPostRwlockWrlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	UnblockThread(curr_thd_id);
	//set the vector clock
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	curr_vc->Join(&rwlock_meta->vc);

	rwlock_meta->SetWrlockOwner(curr_thd_id);
	rwlock_meta->ref++;
}

void Verifier::ProcessPreRwlockUnlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	rwlock_meta->ref--;
	rwlock_meta->wait_vc.Join(curr_vc);

	if(rwlock_meta->ref==0) {
		rwlock_meta->vc=rwlock_meta->wait_vc;
		rwlock_meta->wait_vc.Clear();
	}

	curr_vc->Increment(curr_thd_id);
}

void Verifier::ProcessPostRwlockUnlock(thread_t curr_thd_id,
	RwlockMeta *rwlock_meta)
{
	//we do not know rdlock or wrlock
	rwlock_meta->SetWrlockOwner(0);
	rwlock_meta->RemoveRdlockOwner(curr_thd_id);
}

inline void Verifier::ProcessFree(MutexMeta *mutex_meta)
{
	delete mutex_meta;
}

inline void Verifier::ProcessFree(RwlockMeta *rwlock_meta)
{
	delete rwlock_meta;
}

inline void Verifier::ProcessFree(BarrierMeta *barrier_meta)
{
	delete barrier_meta;
}

inline void Verifier::ProcessFree(SemMeta *sem_meta)
{
	delete sem_meta;
}

/**
 * handle cond_signal and cond_broadcast
 */ 
void Verifier::ProcessSignal(thread_t curr_thd_id,CondMeta *cond_meta)
{
	VectorClock *curr_vc=thd_vc_map_[curr_thd_id];
	//merge the known waiters' vc
	for(CondMeta::ThreadVectorClockMap::iterator iter=
		cond_meta->wait_table.begin();iter!=cond_meta->wait_table.end();
		iter++)
		curr_vc->Join(&iter->second);
	// INFO_FMT_PRINT("===========current vc:[%s]============",curr_vc->ToString().c_str());
	//all potential waiters should know the newest vc from the signaler
	for(CondMeta::ThreadVectorClockMap::iterator iter=
		cond_meta->wait_table.begin();
		iter!=cond_meta->wait_table.end();iter++)
		cond_meta->signal_table[iter->first]=*curr_vc;
	curr_vc->Increment(curr_thd_id);
}

inline void Verifier::ProcessFree(CondMeta *cond_meta)
{
	delete cond_meta;
}
//
void Verifier::HandleNoRace(thread_t curr_thd_id)
{
// INFO_PRINT("=================handle no race===================\n");
	PostponeThread(curr_thd_id);
}
//
void Verifier::HandleRace(std::map<thread_t,uint8> &pp_thd_map,thread_t curr_thd_id)
{
// INFO_PRINT("=================handle race===================\n");
	PostponeThreadSet pp_thds;
	//keep up threads
	PostponeThreadSet keep_up_thds;
	//harmful threads
	PostponeThreadSet hrm_thds;

	for(std::map<thread_t,uint8>::iterator iter=pp_thd_map.begin();
		iter!=pp_thd_map.end();iter++) {
		//pp_thd_map may exist current thread 
		if(iter->first!=curr_thd_id) {
			//harmful threads should go through later
			if(HARMFUL_THREAD(iter->second)) {
				hrm_thds.insert(iter->first);
				continue;
			}
			pp_thds.insert(iter->first);
			if(FULLY_VERIFIED(iter->second))
				keep_up_thds.insert(iter->first);
		}
	}

	//
	if(HARMFUL_THREAD(pp_thd_map[curr_thd_id])) {
		INFO_PRINT("++++++++++++++\n");
		WakeUpPostponeThreadSet(pp_thds);
		PostponeThread(curr_thd_id);
		return ;
	}
	//postponed thread set at least contains a harmless thread
	//wake up postpone thread set,postpone current thread
	if(!RandomBool()) {
		WakeUpPostponeThreadSet(pp_thds);
		//the accessed pstmt of current thread has been verified fully 
		if(FULLY_VERIFIED(pp_thd_map[curr_thd_id]))
			return KeepupThread(curr_thd_id);
		else
			PostponeThread(curr_thd_id);
	}
	//must free the lock if current thread execute continuously
	else {
		WakeUpPostponeThreadSet(keep_up_thds);
		//bring the postponed thread's metas into history metas
		KeepupThread(curr_thd_id);
	}
}

/**
 * do not postpone the thread
 */
void Verifier::KeepupThread(thread_t curr_thd_id)
{
	ClearPostponedThreadMetas(curr_thd_id);
	VerifyUnlock();
}

/**
 * must free lock at each branch of the procedure
 */
void Verifier::PostponeThread(thread_t curr_thd_id)
{
// INFO_FMT_PRINT("=================postpone thread:[%lx]===================\n",curr_thd_id);
// INFO_FMT_PRINT("=================avail_thd_set_ size:[%ld]===================\n",avail_thd_set_.size());
	InternalLock();
	//current thread is the only available thread, others may be blocked by some sync
	//operations, we should not postpone it
	if(avail_thd_set_.size()==1 && pp_thd_set_.empty()) {
		//bring the postponed thread's metas into history metas
		ClearPostponedThreadMetas(curr_thd_id);
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
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME,&ts);
	ts.tv_sec+=2;
	thd_smp_map_[curr_thd_id]->TimedWait(&ts);

	//after waking up
	InternalLock();
	pp_thd_set_.erase(curr_thd_id);
	avail_thd_set_.insert(curr_thd_id);
	//bring the postponed thread's metas into history metas
	ClearPostponedThreadMetas(curr_thd_id);
	InternalUnlock();

// INFO_FMT_PRINT("=================after wait:[%lx]===================\n",curr_thd_id);
}

thread_t Verifier::ChooseRandomThreadAfterAllUnavailable()
{
	if(pp_thd_set_.empty())
		return 0;
	thread_t thd_id=RandomThread(pp_thd_set_);
//INFO_FMT_PRINT("=================needed to wakeup:[%lx]===================\n",thd_id);
	// DEBUG_ASSERT(thd_smp_map_[thd_id]);
	// if(thd_smp_map_[thd_id]->IsWaiting())
	// 	thd_smp_map_[thd_id]->Post();
	WakeUpPostponeThread(thd_id);
	return thd_id;
}

void Verifier::WakeUpPostponeThread(thread_t thd_id)
{
	thd_smp_map_[thd_id]->Post();
	// pp_thd_set_.erase(thd_id);
	// avail_thd_set_.insert(thd_id);
}

void Verifier::WakeUpPostponeThreadSet(PostponeThreadSet &pp_thds)
{
//INFO_FMT_PRINT("=================wakeup pp_thds size:[%ld]===================\n",pp_thds->size());
	for(PostponeThreadSet::iterator iter=pp_thds.begin();iter!=pp_thds.end();
		iter++) {
		// if(thd_smp_map_[*iter]->IsWaiting())
		// 	thd_smp_map_[*iter]->Post();
		WakeUpPostponeThread(*iter);
		//clear postponed threads' corresponding metas
		// delete thd_metas_map_[*iter];
		// thd_metas_map_.erase(*iter);
	}
}


void Verifier::ClearPostponedThreadMetas(thread_t curr_thd_id) {
	if(thd_ppmetas_map_.find(curr_thd_id)==thd_ppmetas_map_.end() ||
		thd_ppmetas_map_[curr_thd_id]->empty())
		return ;
	MAP_KEY_NOTFOUND_NEW(thd_metas_map_,curr_thd_id,MetaSet);
	std::copy(thd_ppmetas_map_[curr_thd_id]->begin(),
		thd_ppmetas_map_[curr_thd_id]->end(),
		inserter(*thd_metas_map_[curr_thd_id],
		thd_metas_map_[curr_thd_id]->end()));
	delete thd_ppmetas_map_[curr_thd_id];	
	thd_ppmetas_map_.erase(curr_thd_id);
}

 
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

/**
 * pp_thds contains result of the raced postponed threads
 */
Verifier::VERIFY_RESULT 
Verifier::WaitVerificationAndHistoryDetection(PStmt *first_pstmt,
	address_t start_addr,address_t end_addr,PStmt *second_pstmt,Inst *inst,
	thread_t curr_thd_id,RaceEventType type,std::map<thread_t,uint8> &pp_thd_map)
{
	// if(pstmt_metas_map_.find(first_pstmt)==pstmt_metas_map_.end() || 
	// 	pstmt_metas_map_[first_pstmt]==NULL)
	// 	return ;
	MetaSet *first_metas=pstmt_metas_map_[first_pstmt];
	MAP_KEY_NOTFOUND_NEW(pstmt_metas_map_,second_pstmt,MetaSet);
	//MAP_KEY_NOTFOUND_NEW(thd_metas_map_,curr_thd_id,MetaSet);
	MAP_KEY_NOTFOUND_NEW(thd_ppmetas_map_,curr_thd_id,MetaSet);
	MetaSet *second_metas=pstmt_metas_map_[second_pstmt];
	PostponeThreadSet tmp_pp_thds;
	bool flag=false;
	VERIFY_RESULT res=NONE_SHARED;
 	timestamp_t curr_thd_clk=thd_vc_map_[curr_thd_id]->GetClock(curr_thd_id);

	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
		Meta *meta=GetMeta(iaddr);
		if(meta->RecentMetaSnapshot(curr_thd_id,curr_thd_clk,inst)) {
			res=REDUDANT;
			continue;
		}
		//corresponding pstmt access the shared meta
		if(first_metas->find(meta)!=first_metas->end()) {

// INFO_FMT_PRINT("===========first pstmt line:[%d],curr addr:[%lx]=============\n",
// 	first_pstmt->line_,meta->addr);
			//verify the postponed thread's metas
			for(ThreadMetasMap::iterator iter=thd_ppmetas_map_.begin();
				iter!=thd_ppmetas_map_.end();iter++) {
				//check meta only in postponed thread
				if(pp_thd_set_.find(iter->first)!=pp_thd_set_.end() && 
					iter->second!=NULL && 
					iter->second->find(meta)!=iter->second->end()) {
					//traverse current thread's last snapshot
					MetaSnapshot *meta_ss=meta->LastestMetaSnapshot(iter->first);

					//filter irrelevant postponed thread or raced inst pair
					if(meta_ss->pstmt!=first_pstmt ||
						meta->RacedInstPair(meta_ss->inst,inst)) {
						res=REDUDANT;
						continue ;						
					}
					if(type==RACE_EVENT_WRITE) {
						flag=true;
						res=RACY;
						tmp_pp_thds.insert(iter->first);
						if(meta_ss->type==RACE_EVENT_WRITE) {
							PrintDebugRaceInfo(meta,WRITETOWRITE,iter->first,
								meta_ss->inst,curr_thd_id,inst);
						}
						else {
							PrintDebugRaceInfo(meta,READTOWRITE,iter->first,
								meta_ss->inst,curr_thd_id,inst);
						}
						//harmful data race
						thread_t hrm_thd_id=ProcessHarmfulRace(meta,iter->first,
							meta_ss->inst,meta_ss->type,curr_thd_id,inst,type);
						if(hrm_thd_id!=0) {
							SET_HARMFUL_THREAD(pp_thd_map[hrm_thd_id]);
							INFO_FMT_PRINT("============wait harmful race thd_id:[%lx]\n",hrm_thd_id);
						}
						//add race to race db
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
							//harmful data race
							thread_t hrm_thd_id=ProcessHarmfulRace(meta,iter->first,
								meta_ss->inst,meta_ss->type,curr_thd_id,inst,type);
							if(hrm_thd_id!=0) {
								SET_HARMFUL_THREAD(pp_thd_map[hrm_thd_id]);
								INFO_FMT_PRINT("============wait harmful race thd_id:[%lx]\n",hrm_thd_id);
							}
							//add race to race db
							ReportRace(meta,iter->first,meta_ss->inst,meta_ss->type,
								curr_thd_id,inst,type);
							meta->AddRacedInstPair(meta_ss->inst,inst);
						}
						else {
							res=SHARED_READ;
						}
					}// else if(type==RACE_EVENT_READ)
				}
			}// verify the postponed thread's metas

			//if history race analysis is unavailable
			if(!HISTORY_RACE_ANALYSIS(flags_))
				continue;

			//postponed racing metas will be ignored if it has been verified racy
			//detect the history raced metas
			for(ThreadMetasMap::iterator iter=thd_metas_map_.begin();
				iter!=thd_metas_map_.end();iter++) {
				if(iter->first!=curr_thd_id && iter->second!=NULL && 
					iter->second->find(meta)!=iter->second->end()) {
					if(meta->meta_ss_map.find(iter->first)==meta->meta_ss_map.end())
						continue;
					MetaSnapshotDeque *meta_ss_deq=meta->meta_ss_map[iter->first];
					//traverse thread history snapshot
					for(MetaSnapshotDeque::iterator iiter=meta_ss_deq->begin();
						*iiter && iiter!=meta_ss_deq->end();iiter++) {
						MetaSnapshot *meta_ss=*iiter;

						if(meta->RacedInstPair(meta_ss->inst,inst))
							continue ;
						//report history race and add to race db
						RaceType race_type=HistoryRace(meta_ss,iter->first,curr_thd_id,
							type);
						if(race_type==WRITETOWRITE || race_type==READTOWRITE ||
							race_type==WRITETOREAD) {
							PrintDebugRaceInfo(meta,race_type,iter->first,meta_ss->inst,
								curr_thd_id,inst);
							//harmful race
							thread_t hrm_thd_id=ProcessHarmfulRace(meta,iter->first,meta_ss->inst,
								meta_ss->type,curr_thd_id,inst,type);
							if(hrm_thd_id!=0)
								INFO_FMT_PRINT("============historical harmful race thd_id:[%lx]\n",hrm_thd_id);
							
							ReportRace(meta,iter->first,meta_ss->inst,meta_ss->type,
								curr_thd_id,inst,type);
							// flag=true;
							meta->AddRacedInstPair(meta_ss->inst,inst);
							//remove the raced pstmt pair mapping
							prace_db_->RemoveRelationMapping(meta_ss->pstmt,second_pstmt);
							if(prace_db_->HasFullyVerified(meta_ss->pstmt)) {
								if(pstmt_metas_map_.find(meta_ss->pstmt)!=
									pstmt_metas_map_.end()) {
									delete pstmt_metas_map_[meta_ss->pstmt];
									pstmt_metas_map_.erase(meta_ss->pstmt);
								}
							}
							if(prace_db_->HasFullyVerified(second_pstmt)) {
								if(pstmt_metas_map_.find(second_pstmt)!=
									pstmt_metas_map_.end()) {
									delete pstmt_metas_map_[second_pstmt];
									pstmt_metas_map_.erase(second_pstmt);
								}
							}
						}
					}// traverse thread history snapshot
				}
			}// detect the history raced metas
 		}// corresponding pstmt access the shared meta
 		else {
 			res=NONE_SHARED;
 		}
 		//add current thread's snapshot
 		AddMetaSnapshot(meta,curr_thd_id,curr_thd_clk,type,inst,second_pstmt);
 		DEBUG_ASSERT(second_metas);
		second_metas->insert(meta);
		// thd_metas_map_[curr_thd_id]->insert(meta);
		thd_ppmetas_map_[curr_thd_id]->insert(meta);
	}
	
	//verify postponed raced meta
	if(flag) {
		prace_db_->RemoveRelationMapping(first_pstmt,second_pstmt);
		if(prace_db_->HasFullyVerified(first_pstmt)) {
			delete pstmt_metas_map_[first_pstmt];
			pstmt_metas_map_.erase(first_pstmt);
			//postponed threads of the first pstmt need not to be postponed
			for(PostponeThreadSet::iterator iter=tmp_pp_thds.begin();
				iter!=tmp_pp_thds.end();iter++) {
				// pp_thd_map[*iter]=false;
				SET_FULLY_VERIFIED(pp_thd_map[*iter]);
			}
		//first pstmt has not been fully verified
		} else {
			for(PostponeThreadSet::iterator iter=tmp_pp_thds.begin();
				iter!=tmp_pp_thds.end();iter++) {
				// pp_thd_map[*iter]=true;
			}
		}

		if(prace_db_->HasFullyVerified(second_pstmt)) {
			delete pstmt_metas_map_[second_pstmt];
			pstmt_metas_map_.erase(second_pstmt);
			//second pstmt has been fully verified
			// pp_thd_map[curr_thd_id]=false;
			SET_FULLY_VERIFIED(pp_thd_map[curr_thd_id]);
		}
	}
	return res;
}

void Verifier::PrintDebugRaceInfo(Meta *meta,RaceType race_type,thread_t t1,
	Inst *i1,thread_t t2,Inst *i2)
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
	DEBUG_FMT_PRINT_SAFE("%s race detected \n",race_type_name);
	DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",meta->addr);
	DEBUG_FMT_PRINT_SAFE("  first thread = [%lx] , inst = [%s]\n",t1,
		i1->ToString().c_str());
	thd_callstack_table_[t1]->PrintCallStack();
	DEBUG_FMT_PRINT_SAFE("  second thread = [%lx] , inst = [%s]\n",t2,
		i2->ToString().c_str());
	thd_callstack_table_[t2]->PrintCallStack();
	DEBUG_FMT_PRINT_SAFE("%s%s\n",SEPARATOR,SEPARATOR);
}

//this function is only invoked at the basis of the determinted data race and
//is applied during the wait verification.
thread_t Verifier::ProcessHarmfulRace(Meta *meta,thread_t tid1,Inst *i1,RaceEventType t1,
	thread_t tid2,Inst *i2,RaceEventType t2)
{
	if(HARMFUL_RACE_ANALYSIS(flags_)) {
		MemMeta mem_meta=mem_bug_->CreateMemMeta(meta->addr);
		MemAccess mem_acc1=mem_bug_->CreateMemAccess(tid1,i1,t1);
		MemAccess mem_acc2=mem_bug_->CreateMemAccess(tid2,i2,t2);
		return mem_bug_->ProcessHarmfulRace(&mem_meta,&mem_acc1,&mem_acc2);
	}
	return 0;
}

}// namespace race