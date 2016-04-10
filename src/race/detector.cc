#include "race/detector.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

namespace race {

std::map<std::string,Detector::EventHandle> Detector::event_handle_table;

Detector::Detector():internal_lock_(NULL),race_db_(NULL),unit_size_(4),
	filter_(NULL),vc_mem_size_(0),adhoc_sync_(NULL),loop_db_(NULL),
	cond_wait_db_(NULL)
{}

Detector::~Detector() 
{
	delete internal_lock_;
	delete filter_;
	for(std::map<thread_t,VectorClock *>::iterator it=curr_vc_map_.begin();
		it!=curr_vc_map_.end();)
		curr_vc_map_.erase(it++);
	if(adhoc_sync_)
		delete adhoc_sync_;
	if(loop_db_)
		delete loop_db_;
	if(cond_wait_db_)
		delete cond_wait_db_;
}

void Detector::SaveStatistics(const char *file_name)
{
	for(std::map<thread_t,VectorClock *>::iterator it=curr_vc_map_.begin();
		it!=curr_vc_map_.end();it++)
		vc_mem_size_+=it->second->GetMemSize();

	//split the director and name
	// string tmp(file_name);
	// size_t found=tmp.find_last_of("/");
	// string &dir=tmp.substr(0,found);
	// if(!dir.empty() && opendir(dir.c_str())==NULL)
	// 	mkdir(dir.c_str(),0775);

	std::fstream out(file_name,std::ios::out | std::ios::app);
	out<<"write insts: "<<GetWriteInsts()<<std::endl;
	out<<"read insts:  "<<GetReadInsts()<<std::endl;
	out<<"locks:       "<<GetLocks()<<std::endl;
	out<<"barriers:    "<<GetBarriers()<<std::endl;
	out<<"condvars:    "<<GetCondVars()<<std::endl;
	out<<"semaphores:  "<<GetSemaphores()<<std::endl;
	out<<"volatiles:   "<<GetVolatiles()<<std::endl;

	out<<"vc_joins:    "<<VectorClock::vc_joins_<<std::endl;
	out<<"vc_assigns:  "<<VectorClock::vc_assigns_<<std::endl;
	out<<"vc_hb_cmps:  "<<VectorClock::vc_hb_cmps_<<std::endl;
	out<<"vc_mem_size: "<<vc_mem_size_<<std::endl;

	out<<"ls_itsecs:   "<<LockSet::ls_itsecs_<<std::endl;
	out<<"ls_assigns:  "<<LockSet::ls_assigns_<<std::endl;
	out<<"ls_allocas:  "<<LockSet::ls_allocas_<<std::endl;
	out<<"ls_adds:     "<<LockSet::ls_adds_<<std::endl;
	out.close();
}

void Detector::PrintDebugRaceInfo(const char *detector_name,RaceType race_type,
	Meta *meta,thread_t curr_thd_id,Inst *curr_inst)
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
	DEBUG_FMT_PRINT_SAFE("%s%s DETECTOR%s\n",SEPARATOR,detector_name,SEPARATOR);
	DEBUG_FMT_PRINT_SAFE("%s race detected [T%lx]\n",race_type_name,curr_thd_id);
	DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n",meta->addr);
	DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n",curr_inst->ToString().c_str());
	DEBUG_FMT_PRINT_SAFE("%s%s DETECTOR%s\n",SEPARATOR,detector_name,SEPARATOR);
}

void Detector::Register()
{
	knob_->RegisterInt("unit_size_","the monitoring granularity in bytes","4");
	knob_->RegisterStr("loop_range_lines","the loop's start line and end line",
		"0");
	knob_->RegisterStr("exiting_cond_lines","spin reads in each loop",
		"0");
	knob_->RegisterStr("cond_wait_lines","condition variable relevant signal"
		" and wait region","0");
}

void Detector::Setup(Mutex *lock,RaceDB *race_db)
{
	internal_lock_=lock;
	race_db_=race_db;
	unit_size_=knob_->ValueInt("unit_size_");
	filter_=new RegionFilter(internal_lock_->Clone());

	//set analyzer descriptor
	desc_.SetHookBeforeMem();
	desc_.SetHookPthreadFunc();
	desc_.SetHookMallocFunc();
	desc_.SetHookAtomicInst();
	desc_.SetHookCallReturn();

	//dynamic ad-hoc
	if(knob_->ValueStr("loop_range_lines").compare("0")!=0) {
		// LoadLoops();
		adhoc_sync_=new AdhocSync;
		if(!adhoc_sync_->LoadLoops(knob_->ValueStr("loop_range_lines").c_str())) {
			delete adhoc_sync_;
			adhoc_sync_=NULL;
		}
	}
	//spinning read loop
	if(knob_->ValueStr("exiting_cond_lines").compare("0")!=0) {
		loop_db_=new LoopDB(race_db_);
		if(!loop_db_->LoadSpinReads(knob_->ValueStr("exiting_cond_lines").c_str())) {
			delete loop_db_;
			loop_db_=NULL;
		}
	}
	//cond_wait
	if(knob_->ValueStr("cond_wait_lines").compare("0")!=0) {
		cond_wait_db_=new CondWaitDB;
		if(!cond_wait_db_->LoadCondWait(knob_->ValueStr("cond_wait_lines").c_str())) {
			delete cond_wait_db_;
			cond_wait_db_=NULL;
		}
	}
	//parallelize the detection 
	if(knob_->ValueInt("parallel_detector_number")>0) {
		REGISTER_EVENT_HANDLE(ImageLoad);
		REGISTER_EVENT_HANDLE(ImageUnload);
		REGISTER_EVENT_HANDLE(ThreadStart);
		REGISTER_EVENT_HANDLE(ThreadExit);
		REGISTER_EVENT_HANDLE(BeforeMemRead);
		REGISTER_EVENT_HANDLE(BeforeMemWrite);
		REGISTER_EVENT_HANDLE(BeforeAtomicInst);
		REGISTER_EVENT_HANDLE(AfterAtomicInst);
		REGISTER_EVENT_HANDLE(AfterPthreadCreate);
		REGISTER_EVENT_HANDLE(BeforePthreadJoin);
		REGISTER_EVENT_HANDLE(AfterPthreadJoin);
		REGISTER_EVENT_HANDLE(BeforeCall);
		REGISTER_EVENT_HANDLE(AfterCall);
		REGISTER_EVENT_HANDLE(BeforeReturn);
		REGISTER_EVENT_HANDLE(AfterReturn);
		REGISTER_EVENT_HANDLE(BeforePthreadMutexTryLock);
		REGISTER_EVENT_HANDLE(BeforePthreadMutexLock);
		REGISTER_EVENT_HANDLE(AfterPthreadMutexLock);
		REGISTER_EVENT_HANDLE(BeforePthreadMutexUnlock);
		REGISTER_EVENT_HANDLE(AfterPthreadMutexTryLock);
		REGISTER_EVENT_HANDLE(BeforePthreadRwlockRdlock);
		REGISTER_EVENT_HANDLE(AfterPthreadRwlockRdlock);
		REGISTER_EVENT_HANDLE(BeforePthreadRwlockWrlock);
		REGISTER_EVENT_HANDLE(AfterPthreadRwlockWrlock);
		REGISTER_EVENT_HANDLE(BeforePthreadRwlockUnlock);
		REGISTER_EVENT_HANDLE(BeforePthreadRwlockTryRdlock);
		REGISTER_EVENT_HANDLE(AfterPthreadRwlockTryRdlock);
		REGISTER_EVENT_HANDLE(BeforePthreadRwlockTryWrlock);
		REGISTER_EVENT_HANDLE(AfterPthreadRwlockTryWrlock);
		REGISTER_EVENT_HANDLE(BeforePthreadCondSignal);
		REGISTER_EVENT_HANDLE(BeforePthreadCondBroadcast);
		REGISTER_EVENT_HANDLE(BeforePthreadCondWait);
		REGISTER_EVENT_HANDLE(AfterPthreadCondWait);
		REGISTER_EVENT_HANDLE(BeforePthreadCondTimedwait);
		REGISTER_EVENT_HANDLE(AfterPthreadCondTimedwait);
		REGISTER_EVENT_HANDLE(AfterMalloc);
		REGISTER_EVENT_HANDLE(AfterCalloc);
		REGISTER_EVENT_HANDLE(BeforeRealloc);
		REGISTER_EVENT_HANDLE(AfterRealloc);
		REGISTER_EVENT_HANDLE(BeforeFree);
		REGISTER_EVENT_HANDLE(BeforePthreadBarrierWait);
		REGISTER_EVENT_HANDLE(AfterPthreadBarrierWait);
		REGISTER_EVENT_HANDLE(BeforeSemPost);
		REGISTER_EVENT_HANDLE(BeforeSemWait);
		REGISTER_EVENT_HANDLE(AfterSemWait);
	}
}

void Detector::ImageLoad(Image *image, address_t low_addr, address_t high_addr,
	address_t data_start, size_t data_size,address_t bss_start, size_t bss_size) 
{
	DEBUG_ASSERT(low_addr && high_addr && high_addr > low_addr);
	if(data_start) {
		DEBUG_ASSERT(data_size);
		AllocAddrRegion(data_start,data_size);
	}
	if(bss_start) {
		DEBUG_ASSERT(bss_size);
		AllocAddrRegion(bss_start,bss_size);
	}
}

void Detector::ImageUnload(Image *image,address_t low_addr, address_t high_addr,
	address_t data_start, size_t data_size,address_t bss_start, size_t bss_size)
{
	DEBUG_ASSERT(low_addr);
	if(data_start)
		FreeAddrRegion(data_start);
	if(bss_start)
		FreeAddrRegion(bss_start);
}

void Detector::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	//create thread local vector clock and lockset
	VectorClock *curr_vc=new VectorClock;
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	//init vector clock
	curr_vc->Increment(curr_thd_id);
	if(parent_thd_id!=INVALID_THD_ID) {
		//not the main thread
		VectorClock *parent_vc=curr_vc_map_[parent_thd_id];
		DEBUG_ASSERT(parent_vc);
		curr_vc->Join(parent_vc);
		parent_vc->Increment(parent_thd_id);
	}
	curr_vc_map_[curr_thd_id]=curr_vc;
	//init atomic map
	atomic_map_[curr_thd_id]=false;
}

void Detector::ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,NULL);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,NULL);
}

void Detector::BeforeMemRead(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr, size_t size)
{
	ReadInstCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(FilterAccess(addr))
		return;
	if(atomic_map_[curr_thd_id])
		return;
	// //process write->spinning read sync
	// if(loop_db_)
	// 	ProcessSRLSync(curr_thd_id,inst);

	//do not specify the checked memory size
	if(unit_size_==0) {
		Meta *meta=GetMeta(addr);
		DEBUG_ASSERT(meta);
		ProcessRead(curr_thd_id,meta,inst);
		return ;
	}
	
	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);

	// if(cond_wait_db_) {
	// 	//handle the previous signal/broadcast->cond_wait sync firstly
	// 	ProcessCWLSync(curr_thd_id,inst);
	// 	//do the signal/broadcast->cond_wait sync analysis
	// 	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
	// 		//process cond wait read
	// 		if(ProcessCondWaitRead(curr_thd_id,inst,iaddr))
	// 			break;
	// 	}
	// }
	
	//use the cyclic counting to identify spinning read loop
	std::set<AdhocSync::ReadMeta *> rd_metas;
	AdhocSync::WriteMeta *wr_meta=NULL;
	if(adhoc_sync_) {
		wr_meta=ProcessAdhocRead(curr_thd_id,inst,start_addr,end_addr,rd_metas);
	}
	// if wr_meta exists, which indidates this read is the last read in loop
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		Meta *meta=GetMeta(iaddr);
		DEBUG_ASSERT(meta);
		ProcessRead(curr_thd_id,meta,inst);
		//remove false races
		if(adhoc_sync_ && !rd_metas.empty() && wr_meta) {
			INFO_PRINT("=================adhoc identify==============\n");
			for(std::set<AdhocSync::ReadMeta *>::iterator iter=rd_metas.begin();
				iter!=rd_metas.end();iter++) {
				race_db_->RemoveRace(wr_meta->lastest_thd_id,wr_meta->inst,
					RACE_EVENT_WRITE,curr_thd_id,(*iter)->inst,RACE_EVENT_READ,false);
				race_db_->RemoveRace(curr_thd_id,(*iter)->inst,RACE_EVENT_READ,
					wr_meta->lastest_thd_id,wr_meta->inst,RACE_EVENT_WRITE,false);
			}
		}
	}
}

void Detector::BeforeMemWrite(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr, size_t size)
{
	WriteInstCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(FilterAccess(addr))
		return ;
	if(atomic_map_[curr_thd_id])
		return ;

	// //process write->spinning read sync
	// if(loop_db_)
	// 	ProcessSRLSync(curr_thd_id,inst);

	if(unit_size_==0) {
		Meta *meta=GetMeta(addr);
		DEBUG_ASSERT(meta);
		ProcessWrite(curr_thd_id,meta,inst);
		return ;
	}

	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);

	// if(cond_wait_db_) {
	// 	//handle the previous signal/broadcast->cond_wait sync firstly
	// 	ProcessCWLSync(curr_thd_id,inst);
	// 	//do the signal/broadcast->cond_wait sync analysis
	// 	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr+=unit_size_) {
	// 		ProcessLockSignalWrite(curr_thd_id,iaddr);
	// 		ProcessCWLCalledFuncWrite(curr_thd_id,iaddr);
	// 	}
	// }
	//keep the lastest write
	if(adhoc_sync_) {
		adhoc_sync_->AddOrUpdateWriteMeta(curr_thd_id,curr_vc_map_[curr_thd_id],
			inst,start_addr,end_addr);
	}

	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		Meta *meta=GetMeta(iaddr);
		DEBUG_ASSERT(meta);
		ProcessWrite(curr_thd_id,meta,inst);
	}
}
//atomic inst doesn't need to be considered
void Detector::BeforeAtomicInst(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,std::string type, address_t addr)
{
	VolatileCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	atomic_map_[curr_thd_id]=true;
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}
//after going through the atomic inst ,we also recover mark for 
//next instruction
void Detector::AfterAtomicInst(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,std::string type, address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	atomic_map_[curr_thd_id]=false;
}

void Detector::BeforePthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	VectorClock *child_vc=curr_vc_map_[child_thd_id];
	curr_vc->Join(child_vc);
	curr_vc->Increment(curr_thd_id);
	// remove the child thread vc
	if(loop_db_ || cond_wait_db_)
		return ;
	delete curr_vc_map_[child_thd_id];
	curr_vc_map_.erase(child_thd_id);		
}

void Detector::AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
	Inst *inst,thread_t childThdId) 
{ }

void Detector::BeforeCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_) {
		ProcessSRLSync(curr_thd_id,inst);	
		//exiting condtion line called function	
		if(loop_db_->SpinReadCalledFunc(inst)) {
			loop_db_->SetSpinReadCalledFunc(curr_thd_id,inst,true);
		}
	}
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
		//exiting condtion line called function	
		if(cond_wait_db_->CondWaitCalledFunc(inst)) {
			cond_wait_db_->SetCondWaitCalledFunc(curr_thd_id,inst,true);
		}	
	}
}
void Detector::AfterCall(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target,address_t ret)
{

}
void Detector::BeforeReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_) {
		ProcessSRLSync(curr_thd_id,inst);		
	}
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}
void Detector::AfterReturn(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    Inst *inst,address_t target)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_) {
		loop_db_->RemoveSpinReadCalledFunc(curr_thd_id);
	}
	if(cond_wait_db_)
		cond_wait_db_->RemoveCondWaitCalledFunc(curr_thd_id);
}

void Detector::BeforePthreadMutexTryLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::BeforePthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
	}
}

//After acquire the the lock
void Detector::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	if(cond_wait_db_)
		cond_wait_db_->AddLastestLock(curr_thd_id,addr);
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}

void Detector::AfterPthreadMutexTryLock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val) 
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

//Before release the lock
void Detector::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
  	ScopedLock locker(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
		//remove the unactived lock writes meta
		cond_wait_db_->RemoveUnactivedLockWritesMeta(curr_thd_id,
			addr);
	}
  	MutexMeta *meta = GetMutexMeta(addr);
  	DEBUG_ASSERT(meta);
  	ProcessUnlock(curr_thd_id, meta);
}

void Detector::BeforePthreadRwlockRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	//?????
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}

void Detector::BeforePthreadRwlockWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	if(cond_wait_db_)
		cond_wait_db_->AddLastestLock(curr_thd_id,addr);
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}

void Detector::BeforePthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
}

void Detector::BeforePthreadRwlockTryWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterPthreadRwlockTryWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val) 
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockWrlock(curr_thd_id,curr_thd_clk,inst,addr);	
}


void Detector::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
  	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_) {
		ProcessCWLSync(curr_thd_id,inst);
		//remove the unactived lock writes meta
		cond_wait_db_->RemoveUnactivedLockWritesMeta(curr_thd_id,
			addr);
	}
  	MutexMeta *meta = GetMutexMeta(addr);
  	DEBUG_ASSERT(meta);
  	ProcessUnlock(curr_thd_id, meta);
}


void Detector::BeforePthreadCondSignal(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	//active the lock signal meta
	if(cond_wait_db_)
		cond_wait_db_->ActiveLockSignalMeta(curr_thd_id);
	CondMeta *meta=GetCondMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessNotify(curr_thd_id,meta);
}

void Detector::BeforePthreadCondBroadcast(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst, address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	//active the lock signal meta
	if(cond_wait_db_)
		cond_wait_db_->ActiveLockSignalMeta(curr_thd_id);
	CondMeta *meta=GetCondMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessNotify(curr_thd_id,meta);
}

void Detector::BeforePthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t cond_addr, address_t mutex_addr)
{
	CondVarCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	//remove the cond_wait meta
	if(cond_wait_db_)
		cond_wait_db_->RemoveCondWaitMeta(curr_thd_id);
  	//unlock
  	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
  	DEBUG_ASSERT(mutex_meta);
  	ProcessUnlock(curr_thd_id,mutex_meta);

  	//wait
  	CondMeta *cond_meta=GetCondMeta(cond_addr);
  	DEBUG_ASSERT(cond_meta);
  	ProcessPreWait(curr_thd_id,cond_meta);
}

void Detector::AfterPthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr, 
	address_t mutex_addr)
{
	CondVarCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);

  	//wait
  	CondMeta *cond_meta=GetCondMeta(cond_addr);
  	DEBUG_ASSERT(cond_meta);
  	ProcessPostWait(curr_thd_id,cond_meta);

  	//lock
  	MutexMeta *mutex_meta=GetMutexMeta(mutex_addr);
  	DEBUG_ASSERT(mutex_meta);
  	ProcessLock(curr_thd_id,mutex_meta);
}

void Detector::BeforePthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void Detector::AfterPthreadCondTimedwait(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t cond_addr,
    address_t mutex_addr)
{
	AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,
		mutex_addr);
}

void Detector::BeforePthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BarrierCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
	BarrierMeta *meta=GetBarrierMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessPreBarrier(curr_thd_id,meta);
}

void Detector::AfterPthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BarrierCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	BarrierMeta *meta=GetBarrierMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessPostBarrier(curr_thd_id,meta);
}


void Detector::BeforeSemPost(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
	SemMeta *meta=GetSemMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessBeforeSemPost(curr_thd_id,meta);
}

void Detector::BeforeSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(loop_db_)
		ProcessSRLSync(curr_thd_id,inst);
	if(cond_wait_db_)
		ProcessCWLSync(curr_thd_id,inst);
}

void Detector::AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr)
{
	SemaphoreCountIncrease();
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	SemMeta *meta=GetSemMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessAfterSemWait(curr_thd_id,meta);
}

void Detector::AfterMalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, size_t size, address_t addr)
{
	AllocAddrRegion(addr,size);
}

void Detector::AfterCalloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, size_t nmemb, size_t size,address_t addr)
{
	AllocAddrRegion(addr,size * nmemb);
}

void Detector::BeforeRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t ori_addr, size_t size) 
{
  	FreeAddrRegion(ori_addr);
}
void Detector::AfterRealloc(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t ori_addr, size_t size,address_t new_addr) 
{
  	AllocAddrRegion(new_addr, size);
}

void Detector::BeforeFree(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr) 
{
  	FreeAddrRegion(addr);
}

//help functions
void Detector::AllocAddrRegion(address_t addr,size_t size)
{
	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	DEBUG_ASSERT(addr && size);
	filter_->AddRegion(addr,size,false);

}

void Detector::FreeAddrRegion(address_t addr)
{

	ScopedLock lock(internal_lock_,!knob_->ValueInt("parallel_detector_number"));
	if(!addr) return ;
	size_t size=filter_->RemoveRegion(addr,false);
	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);
	//for memory
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		Meta::Table::iterator it=meta_table_.find(iaddr);
		if(it!=meta_table_.end()) {
			//for every meta
			ProcessFree(it->second);
			meta_table_.erase(it);
		}
	}
	//for mutex
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		MutexMeta::Table::iterator it=mutex_meta_table_.find(iaddr);
		if(it!=mutex_meta_table_.end()) {
			//for every mutex meta
			ProcessFree(it->second);
			mutex_meta_table_.erase(it);
		}
	}
	//for condditon variable
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		CondMeta::Table::iterator it=cond_meta_table_.find(iaddr);
		if(it!=cond_meta_table_.end()) {
			//for every cond meta
			ProcessFree(it->second);
			cond_meta_table_.erase(it);
		}
	}
	//for barrier
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		BarrierMeta::Table::iterator it=barrier_meta_table_.find(iaddr);
		if(it!=barrier_meta_table_.end()) {
			//for every barrier meta
			ProcessFree(it->second);
			barrier_meta_table_.erase(it);
		}
	}
	//for semaphore
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		SemMeta::Table::iterator it=sem_meta_table_.find(iaddr);
		if(it!=sem_meta_table_.end()) {
			//for every semaphore meta
			ProcessFree(it->second);
			sem_meta_table_.erase(it);
		}
	}
}


void Detector::ProcessFree(MutexMeta *meta) 
{
	vc_mem_size_+=meta->vc.GetMemSize();
  	delete meta;
}

void Detector::ProcessFree(CondMeta *meta)
{
	for(CondMeta::VectorClockMap::iterator it=meta->wait_table.begin();
		it!=meta->wait_table.end();it++)
		vc_mem_size_+=it->second.GetMemSize();
	for(CondMeta::VectorClockMap::iterator it=meta->signal_table.begin();
		it!=meta->signal_table.end();it++)
		vc_mem_size_+=it->second.GetMemSize();
	delete meta;
}

void Detector::ProcessFree(BarrierMeta *meta)
{
	vc_mem_size_+=meta->wait_vc.GetMemSize();
	delete meta;
}

void Detector::ProcessFree(SemMeta *meta)
{
	vc_mem_size_+=meta->vc.GetMemSize();
	delete meta;
}

Detector::MutexMeta *Detector::GetMutexMeta(address_t iaddr)
{
	MutexMeta::Table::iterator it=mutex_meta_table_.find(iaddr);
	if(it==mutex_meta_table_.end()) {
		MutexMeta *meta=new MutexMeta;
		mutex_meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

Detector::CondMeta *Detector::GetCondMeta(address_t iaddr)
{
	CondMeta::Table::iterator it=cond_meta_table_.find(iaddr);
	if(it==cond_meta_table_.end()) {
		CondMeta *meta=new CondMeta;
		cond_meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

Detector::BarrierMeta *Detector::GetBarrierMeta(address_t iaddr)
{
	BarrierMeta::Table::iterator it=barrier_meta_table_.find(iaddr);
	if(it==barrier_meta_table_.end()) {
		BarrierMeta *meta=new BarrierMeta;
		barrier_meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

Detector::SemMeta *Detector::GetSemMeta(address_t iaddr)
{
	SemMeta::Table::iterator it=sem_meta_table_.find(iaddr);
	if(it==sem_meta_table_.end()) {
		SemMeta *meta=new SemMeta;
		sem_meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void Detector::ReportRace(Meta *meta, thread_t t0, Inst *i0,
	RaceEventType p0, thread_t t1, Inst *i1,RaceEventType p1)
{
	race_db_->CreateRace(meta->addr,t0,i0,p0,t1,i1,p1,false);
}


void Detector::ProcessLock(thread_t curr_thd_id,MutexMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	curr_vc->Join(&meta->vc);

}

void Detector::ProcessUnlock(thread_t curr_thd_id,MutexMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	//update the mutex vector clock
	meta->vc=*curr_vc;
	curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessNotify(thread_t curr_thd_id,CondMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);

	//iterate the wait table,join vector clock
	for(CondMeta::VectorClockMap::iterator it=meta->wait_table.begin();
		it!=meta->wait_table.end();it++)
		curr_vc->Join(&it->second);

	//update signal table
	for(CondMeta::VectorClockMap::iterator it=meta->wait_table.begin();
		it!=meta->wait_table.end();it++)
		meta->signal_table[it->first]=*curr_vc;

	curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessPreWait(thread_t curr_thd_id,CondMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	meta->wait_table[curr_thd_id]=*curr_vc;
	curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessPostWait(thread_t curr_thd_id,CondMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	CondMeta::VectorClockMap::iterator wit=meta->wait_table.find(curr_thd_id);
	DEBUG_ASSERT(wit!=meta->wait_table.end());

	//post_wait may doesn't depend on a signal or broadcast
	//may be alarm signal for wait_timeout or other signals
	//only some thread waiting the condition variable,signal vector clock
	//will be meanginful,so after waiting thread get the signal,both vc
	//we should erase
	meta->wait_table.erase(wit);
	CondMeta::VectorClockMap::iterator sit=meta->signal_table.find(curr_thd_id);
	//do signal 
	if(sit!=meta->signal_table.end()) {
		curr_vc->Join(&sit->second);
		meta->signal_table.erase(sit);
	}
}

void Detector::ProcessPreBarrier(thread_t curr_thd_id,BarrierMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	meta->wait_vc.Join(curr_vc);
	meta->waiter++;
}

void Detector::ProcessPostBarrier(thread_t curr_thd_id,BarrierMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	curr_vc->Join(&meta->wait_vc);
	curr_vc->Increment(curr_thd_id);
	if((--meta->waiter)==0)
		meta->wait_vc.Clear();
}

void Detector::ProcessBeforeSemPost(thread_t curr_thd_id,SemMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	//update the semaphore vector clock
	meta->vc.Join(curr_vc);
	curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessAfterSemWait(thread_t curr_thd_id,SemMeta *meta)
{
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	DEBUG_ASSERT(curr_vc);
	curr_vc->Join(&meta->vc);
	curr_vc->Increment(curr_thd_id);
}

//use the cyclic counting to idenftify spinning read loop
AdhocSync::WriteMeta *Detector::ProcessAdhocRead(thread_t curr_thd_id,Inst *rd_inst,
    address_t start_addr,address_t end_addr,
    std::set<AdhocSync::ReadMeta *> &result)
{
	std::string file_name=rd_inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=rd_inst->GetLine();
	AdhocSync::WriteMeta *wr_meta=NULL;
	AdhocSync::LoopMap &loop_map=adhoc_sync_->GetLoops();
	if(loop_map.find(file_name)!=loop_map.end()) {
		//find the suitable loop
		LoopTable::reverse_iterator iter;
		for(iter=loop_map[file_name]->rbegin();iter!=loop_map[file_name]->rend();
			iter++)
			if(iter->first<=line)
				break;
		if(iter->second.InLoop(line)) {
// INFO_PRINT("=================loop read==============\n");
			wr_meta=adhoc_sync_->WriteReadSync(curr_thd_id,rd_inst,start_addr,
				end_addr);
			if(wr_meta) {
				adhoc_sync_->BuildWriteReadSync(wr_meta,
					curr_vc_map_[wr_meta->GetLastestThread()],curr_vc_map_[curr_thd_id]);
				//some races may have been detected in the write access
				adhoc_sync_->SameAddrReadMetas(curr_thd_id,start_addr,end_addr,result);
			}
		}
	}
	return wr_meta;
}

/**
 * if curr_inst is NULL, which indicates not to check if in the loop scope
 */
void Detector::ProcessSRLSync(thread_t curr_thd_id,Inst *curr_inst)
{
	//process spinning read loop
	thread_t spin_rlt_wrthd=loop_db_->
		GetSpinRelevantWriteThread(curr_thd_id);
	if(spin_rlt_wrthd!=0)
		loop_db_->ProcessWriteReadSync(curr_thd_id,curr_inst,
			curr_vc_map_[spin_rlt_wrthd],curr_vc_map_[curr_thd_id]);
}

void Detector::ProcessSRLRead(thread_t curr_thd_id,Inst *curr_inst,address_t addr)
{
	//handle the previous write->spinning read sync
	ProcessSRLSync(curr_thd_id,curr_inst);
	//if the read of the spin thread
	if(loop_db_->SpinRead(curr_inst,RACE_EVENT_READ) ||
		loop_db_->SpinReadCalledFuncThread(curr_thd_id)) {
		//initialize the spin read meta
		loop_db_->SetSpinReadThread(curr_thd_id,curr_inst);
		loop_db_->SetSpinReadAddr(curr_thd_id,addr);
		//keep the call inst
		if(loop_db_->SpinReadCalledFuncThread(curr_thd_id))
			loop_db_->SetSpinReadCallInst(curr_thd_id);
	}
}

void Detector::ProcessSRLWrite(thread_t curr_thd_id,Inst *curr_inst,address_t addr)
{
	//handle the previous write->spinning read sync
	ProcessSRLSync(curr_thd_id,curr_inst);
	//if there are shared location modification in called function
	// if(loop_db_->SpinReadCalledFuncThread(curr_thd_id)) {
	// 	//not spinning read
	// 	if(loop_db_->SpinReadThread(curr_thd_id) && 
	// 		loop_db_->GetSpinReadAddr(curr_thd_id)==addr) {
	// 		loop_db_->SetSpinReadCalledFunc(curr_thd_id,NULL,false);
	// 		//remove the potential spinning read meta
	// 		loop_db_->RemoveSpinReadMeta(curr_thd_id);			
	// 	}
	// }

	//if there are shared location modification in spinning read loop
	//this spinning read loop is invalid
	if(loop_db_->SpinReadThread(curr_thd_id)) {
		bool valid=true;
		if(loop_db_->SpinReadCalledFuncThread(curr_thd_id)) {
			loop_db_->SetSpinReadCalledFunc(curr_thd_id,NULL,false);
			valid=false;
		}
		if(loop_db_->GetSpinReadAddr(curr_thd_id)==addr)
			valid=false;
		if(!valid) {
			//remove the potential spinning read meta
			loop_db_->RemoveSpinReadMeta(curr_thd_id);
		}			
	}
}
 
void Detector::ProcessCWLCalledFuncWrite(thread_t curr_thd_id,address_t addr)
{
	if(cond_wait_db_->CondWaitCalledFuncThread(curr_thd_id)) {
		if(cond_wait_db_->CondWaitMetaThread(curr_thd_id) &&
			addr==cond_wait_db_->GetCondWaitMetaAddr(curr_thd_id)) {
			cond_wait_db_->SetCondWaitCalledFunc(curr_thd_id,NULL,false);
			//remove the potential cond_wait meta
			cond_wait_db_->RemoveCondWaitMeta(curr_thd_id);
		}
	}
}
 
void Detector::ProcessLockSignalWrite(thread_t curr_thd_id,address_t addr)
{
	// INFO_FMT_PRINT("=========process lock signal write=========\n");
	//current thread should be protected by at least one lock
	address_t lk_addr=0;
	if((lk_addr=cond_wait_db_->GetLastestLock(curr_thd_id))==0)
		return ;
	//add the lock write meta
	cond_wait_db_->AddLockSignalWriteMeta(curr_thd_id,
		*curr_vc_map_[curr_thd_id],addr,lk_addr);
}
 
bool Detector::ProcessCWLRead(thread_t curr_thd_id,Inst *curr_inst,address_t addr)
{
	std::string file_name=curr_inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=curr_inst->GetLine();
	return ProcessCWLRead(curr_thd_id,curr_inst,addr,file_name,line);
}
 
bool Detector::ProcessCWLRead(thread_t curr_thd_id,Inst *curr_inst,
	address_t addr,std::string &file_name,int line)
{
	return cond_wait_db_->ProcessCondWaitRead(curr_thd_id,curr_inst,
		*curr_vc_map_[curr_thd_id],addr,file_name,line);
}

void Detector::ProcessCWLSync(thread_t curr_thd_id,Inst *curr_inst)
{
	cond_wait_db_->ProcessSignalCondWaitSync(curr_thd_id,curr_inst,
		*curr_vc_map_[curr_thd_id]);
}
} //namespace race