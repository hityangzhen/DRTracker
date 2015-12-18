#include "race/detector.h"
#include "core/log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

namespace race {

Detector::Detector():internal_lock_(NULL),race_db_(NULL),
	unit_size_(4),filter_(NULL),vc_mem_size_(0)
{
	adhoc_sync_=new AdhocSync;
}

Detector::~Detector() 
{
	delete internal_lock_;
	delete filter_;
	delete adhoc_sync_;
	for(std::map<thread_t,VectorClock *>::iterator it=curr_vc_map_.begin();
		it!=curr_vc_map_.end();)
		curr_vc_map_.erase(it++);
	//clear loops
	for(LoopMap::iterator iter=loop_map_.begin();iter!=loop_map_.end();
		iter++)
		delete iter->second;
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

	//load loop range lines
	if(knob_->ValueStr("loop_range_lines").compare("0")!=0)
		LoadLoops();
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
	ScopedLock lock(internal_lock_);
	//init vector clock
	curr_vc->Increment(curr_thd_id);
	if(parent_thd_id!=INVALID_THD_ID) {
		//not the main thread
		VectorClock *parent_vc=curr_vc_map_[parent_thd_id];
		DEBUG_ASSERT(parent_vc);
		curr_vc->Join(parent_vc);
		//parent_vc->Increment(parent_thd_id);
	}
	curr_vc_map_[curr_thd_id]=curr_vc;
	//init atomic map
	atomic_map_[curr_thd_id]=false;
}


void Detector::BeforeMemRead(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr, size_t size)
{
	ReadInstCountIncrease();
	ScopedLock lock(internal_lock_);
	if(FilterAccess(addr))
		return;
	if(atomic_map_[curr_thd_id])
		return;

	//do not specify the checked memory size
	if(unit_size_==0) {
		Meta *meta=GetMeta(addr);
		DEBUG_ASSERT(meta);
		ProcessRead(curr_thd_id,meta,inst);
		return ;
	}
	
	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);

	//handle the read in loop
	std::string file_name=inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	AdhocSync::WriteMeta *wr_meta=NULL;
	std::set<AdhocSync::ReadMeta *> rd_metas;
	if(loop_map_.find(file_name)!=loop_map_.end()) {
		//find the suitable loop
		LoopTable::reverse_iterator iter;
		for(iter=loop_map_[file_name]->rbegin();iter!=loop_map_[file_name]->rend();
			iter++)
			if(iter->first<=line)
				break;
		if(iter->second.InLoop(line)) {
INFO_PRINT("=================loop read==============\n");
			wr_meta=adhoc_sync_->WriteReadSync(curr_thd_id,inst,start_addr,
				end_addr);
			if(wr_meta) {
				adhoc_sync_->BuildWriteReadSync(wr_meta,
					curr_vc_map_[wr_meta->GetLastestThread()],curr_vc_map_[curr_thd_id]);
				//some races may have been detected in the write access
				adhoc_sync_->SameAddrReadMetas(curr_thd_id,start_addr,end_addr,
					rd_metas);
			}
		}
	}
	// if wr_meta exists, which indidates this read is the last read in loop
	for(address_t iaddr=start_addr;iaddr<end_addr;iaddr += unit_size_) {
		Meta *meta=GetMeta(iaddr);
		DEBUG_ASSERT(meta);
		ProcessRead(curr_thd_id,meta,inst);
		//remove false races
		if(!rd_metas.empty()) {
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
	ScopedLock lock(internal_lock_);
	if(FilterAccess(addr))
		return ;
	if(atomic_map_[curr_thd_id])
		return ;
	
	if(unit_size_==0) {
		Meta *meta=GetMeta(addr);
		DEBUG_ASSERT(meta);
		ProcessWrite(curr_thd_id,meta,inst);
		return ;
	}

	address_t start_addr=UNIT_DOWN_ALIGN(addr,unit_size_);
	address_t end_addr=UNIT_UP_ALIGN(addr+size,unit_size_);
	//keep the lastest write
	adhoc_sync_->AddOrUpdateWriteMeta(curr_thd_id,curr_vc_map_[curr_thd_id],
		inst,start_addr,end_addr);

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
	ScopedLock lock(internal_lock_);
	atomic_map_[curr_thd_id]=true;
}
//after going through the atomic inst ,we also recover mark for 
//next instruction
void Detector::AfterAtomicInst(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,std::string type, address_t addr)
{
	ScopedLock lock(internal_lock_);
	atomic_map_[curr_thd_id]=false;
}

void Detector::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,thread_t child_thd_id)
{
	ScopedLock lock(internal_lock_);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	VectorClock *child_vc=curr_vc_map_[child_thd_id];
	curr_vc->Join(child_vc);
	curr_vc->Increment(curr_thd_id);
}

void Detector::AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
	Inst *inst,thread_t childThdId) 
{
	ScopedLock lock(internal_lock_);
	curr_vc_map_[currThdId]->Increment(currThdId);
}

//After acquire the the lock
void Detector::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
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
  	ScopedLock locker(internal_lock_);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  	MutexMeta *meta = GetMutexMeta(addr);
  	DEBUG_ASSERT(meta);
  	ProcessUnlock(curr_thd_id, meta);
}



void Detector::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	//?????
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}
void Detector::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr) 
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_) == addr);
	//?????
	MutexMeta *meta=GetMutexMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessLock(curr_thd_id,meta);
}

void Detector::AfterPthreadRwlockTryRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr,int ret_val)
{
	LockCountIncrease();
	if(ret_val!=0)
		return ;
	AfterPthreadRwlockRdlock(curr_thd_id,curr_thd_clk,inst,addr);
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
  	ScopedLock lock(internal_lock_);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  	MutexMeta *meta = GetMutexMeta(addr);
  	DEBUG_ASSERT(meta);
  	ProcessUnlock(curr_thd_id, meta);
}


void Detector::BeforePthreadCondSignal(thread_t curr_thd_id,
    timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	CondMeta *meta=GetCondMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessNotify(curr_thd_id,meta);
}

void Detector::BeforePthreadCondBroadcast(thread_t curr_thd_id,
    timestamp_t curr_thd_clk,Inst *inst, address_t addr)
{
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
	CondMeta *meta=GetCondMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessNotify(curr_thd_id,meta);
}

void Detector::BeforePthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t cond_addr, address_t mutex_addr)
{
	CondVarCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  	DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);

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
	ScopedLock lock(internal_lock_);
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
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	BarrierMeta *meta=GetBarrierMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessPreBarrier(curr_thd_id,meta);
}

void Detector::AfterPthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	BarrierCountIncrease();
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	BarrierMeta *meta=GetBarrierMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessPostBarrier(curr_thd_id,meta);
}


void Detector::BeforeSemPost(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t addr)
{
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr,unit_size_)==addr);
	SemMeta *meta=GetSemMeta(addr);
	DEBUG_ASSERT(meta);
	ProcessBeforeSemPost(curr_thd_id,meta);
}

void Detector::AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
	Inst *inst,address_t addr)
{
	SemaphoreCountIncrease();
	ScopedLock lock(internal_lock_);
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
	ScopedLock lock(internal_lock_);
	DEBUG_ASSERT(addr && size);
	filter_->AddRegion(addr,size,false);

}

void Detector::FreeAddrRegion(address_t addr)
{

	ScopedLock lock(internal_lock_);
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
}

void Detector::LoadLoops()
{
	const char *delimit=" ",*fn=NULL,*sl=NULL,*el=NULL;
	char buffer[200];
	std::fstream in(knob_->ValueStr("loop_range_lines").c_str(),
		std::ios::in);
	while(!in.eof()) {
		in.getline(buffer,200,'\n');
		fn=strtok(buffer,delimit);
		sl=strtok(NULL,delimit);
		el=strtok(NULL,delimit);
		DEBUG_ASSERT(fn && sl && el);
		std::string fn_str(fn);
		if(loop_map_.find(fn_str)==loop_map_.end())
			loop_map_[fn_str]=new LoopTable;
		int sl_int=atoi(sl),el_int=atoi(el);
		loop_map_[fn_str]->insert(std::make_pair(sl_int,Loop(sl_int,el_int)));
	}
	in.close();
}

} //namespace race