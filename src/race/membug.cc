#include "race/membug.h"
#include "core/log.h"
#include <fstream>

namespace race
{
static uint64 FilenameAndLineHash(const char *file_name,int line) 
{
	uint64 key=0;
	while(*file_name)
		key += *file_name++;
	key += line;
	return key;
}

NullPtrDeref::NullPtrDeref()
{}

NullPtrDeref::~NullPtrDeref()
{}

bool NullPtrDeref::ValidPtrReadOrWrite(MemAccess *mem_acc)
{
	std::string file_name=mem_acc->inst->GetFileName();
	int line=mem_acc->inst->GetLine();
	// pointer write access must contains NULL
	if(mem_acc->type==RACE_EVENT_WRITE)
		return null_ptrwr_set_.find(FilenameAndLineHash(file_name.c_str(),
			line))!=null_ptrwr_set_.end();
	else
		return null_ptrrd_set_.find(FilenameAndLineHash(file_name.c_str(),
			line))==null_ptrrd_set_.end();
}

bool NullPtrDeref::Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2)
{
	return ValidPtrReadOrWrite(mem_acc1) & ValidPtrReadOrWrite(mem_acc2);
}

// Load the information about the null pointer read
bool NullPtrDeref::LoadStaticInfo(const char *file_name)
{
	std::fstream in(file_name,std::ios::in);
	if(!in)
		return false;
	/* Suspicious write null to the pointer and infeasible condition check
	   and assignment read.
	   @format filename +' '+ lineno +' '+ flag */
	const char *delimit=" ",*fn,*ln,*fg;
	fn=ln=fg=NULL;
	char buffer[50];
	while(!in.eof()) {
		in.getline(buffer,50,'\n');
		fn=strtok(buffer,delimit);
		if(fn==NULL)
			continue;
		ln=strtok(NULL,delimit);
		fg=strtok(NULL,delimit);

		if(*fg=='0')
			null_ptrwr_set_.insert(FilenameAndLineHash(fn,atoi(ln)));
		else if(*fg=='1')
			null_ptrrd_set_.insert(FilenameAndLineHash(fn,atoi(ln)));
	}
	in.close();
	return true;
}

UnInitRead::UnInitRead():filter_(NULL)
{}

UnInitRead::~UnInitRead()
{
	delete filter_;
	for(MemMetaInitTable::iterator iter=memmeta_init_table_.begin();
		iter!=memmeta_init_table_.end();iter++) {
		delete iter->second;
	}
}

inline void UnInitRead::CreateGlobalRegionFilter(Mutex *lock)
{
	filter_=new RegionFilter(lock);
}

inline void UnInitRead::AddGlobalRegion(address_t region_start,size_t region_size)
{
	DEBUG_ASSERT(region_size);
	filter_->AddRegion(region_start,region_size,false);
}

inline bool UnInitRead::InGlobalRegion(address_t addr)
{
	return filter_->Filter(addr,false);
}

bool UnInitRead::Harmful(MemMeta *meta,MemAccess *mem_acc1,MemAccess *mem_acc2)
{
	if(InGlobalRegion(meta->addr))
		return false;
	if(mem_acc1->type==RACE_EVENT_WRITE) {
		if(HasWrite(meta,mem_acc1->thd_id))
			return false;
		SetInitWrite(meta,mem_acc1->thd_id);
		return true;
	}
	else if(mem_acc2->type==RACE_EVENT_WRITE) {
		if(HasWrite(meta,mem_acc2->thd_id))
			return false;
		SetInitWrite(meta,mem_acc2->thd_id);
		return true;		
	}
	return false;
}

void UnInitRead::SetInitWrite(MemMeta *meta,thread_t thd_id)
{
	if(memmeta_init_table_.find(meta)==memmeta_init_table_.end())
		memmeta_init_table_[meta]=new ThreadFlagMap;
	ThreadFlagMap *thd_flg_map=memmeta_init_table_[meta];
	if(thd_flg_map->find(thd_id)==thd_flg_map->end())
		(*thd_flg_map)[thd_id]=true;
}

bool UnInitRead::HasWrite(MemMeta *meta,thread_t thd_id)
{
	if(memmeta_init_table_.find(meta)==memmeta_init_table_.end())
		return false;
	ThreadFlagMap *thd_flg_map=memmeta_init_table_[meta];
	if(thd_flg_map->find(thd_id)==thd_flg_map->end())
		return false;
	return true;
}

DanglingPtr::DanglingPtr():filter_(NULL)
{}

DanglingPtr::~DanglingPtr()
{
	delete filter_;
}

inline void DanglingPtr::CreateFreedRegionFilter(Mutex *lock)
{
	filter_=new RegionFilter(new NullMutex);
}

inline void DanglingPtr::AddFreedRegion(address_t region_start,size_t region_size)
{
	DEBUG_ASSERT(region_size);
	filter_->AddRegion(region_start,region_size);
}

inline bool DanglingPtr::InFreedRegion(address_t addr)
{
	return filter_->Filter(addr);
}

bool DanglingPtr::DeletePtr(MemAccess *mem_acc)
{
	std::string file_name=mem_acc->inst->GetFileName();
	int line=mem_acc->inst->GetLine();
	return del_ptr_set_.find(FilenameAndLineHash(file_name.c_str(),line))!=
		del_ptr_set_.end();
}

inline thread_t DanglingPtr::Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2)
{
	// One is `delete` operator and one is an access to the ptr.
	if(DeletePtr(mem_acc1) && !DeletePtr(mem_acc2))
		return mem_acc1->thd_id;
	else if(DeletePtr(mem_acc2) && !DeletePtr(mem_acc1))
		return mem_acc2->thd_id;
	return 0;
}

bool DanglingPtr::LoadStaticInfo(const char *file_name)
{
	std::fstream in(file_name,std::ios::in);
	if(!in)
		return false;
	/* Suspicious `delete`operator.
	   @format filename +' '+ lineno */
	const char *delimit=" ",*fn,*ln;
	fn=ln=NULL;
	char buffer[50];
	while(in.eof()) {
		in.getline(buffer,50,'\n');
		fn=strtok(buffer,delimit);
		if(fn==NULL)
			continue;
		ln=strtok(NULL,delimit);
		del_ptr_set_.insert(FilenameAndLineHash(fn,atoi(ln)));
	}
	in.close();
	return true;
}

BufferOverflow::BufferOverflow()
{}

BufferOverflow::~BufferOverflow()
{}

inline bool BufferOverflow::BufferIndex(MemAccess *mem_acc)
{
	std::string file_name=mem_acc->inst->GetFileName();
	int line=mem_acc->inst->GetLine();
	return buf_idx_set_.find(FilenameAndLineHash(file_name.c_str(),line))!=
		buf_idx_set_.end();
}

bool BufferOverflow::Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2)
{
	return mem_acc1->type==RACE_EVENT_READ ? BufferIndex(mem_acc1):
		BufferIndex(mem_acc2);
}

bool BufferOverflow::LoadStaticInfo(const char *file_name)
{
	std::fstream in(file_name,std::ios::in);
	if(!in)
		return false;
	/* Suspicious index access.
	   @format filename +' '+ lineno */ 
	const char *delimit=" ",*fn,*ln;
	fn=ln=NULL;
	char buffer[50];
	while(in.eof()) {
		in.getline(buffer,50,'\n');
		fn=strtok(buffer,delimit);
		if(fn==NULL)
			continue;
		ln=strtok(NULL,delimit);
		buf_idx_set_.insert(FilenameAndLineHash(fn,atoi(ln)));
	}
	in.close();
	return true;
}

MemBug::MemBug(Knob *knob):knob_(knob),null_ptr_deref_(NULL),uninit_read_(NULL),
	dangling_ptr_(NULL),buffer_overflow_(NULL)
{}

MemBug::~MemBug()
{
	if(null_ptr_deref_)
		delete null_ptr_deref_;
	if(uninit_read_)
		delete uninit_read_;
	if(dangling_ptr_)
		delete dangling_ptr_;
	if(buffer_overflow_)
		delete buffer_overflow_;
}

void MemBug::Register()
{
	knob_->RegisterStr("null_ptr_deref","file name of the static information"
		" about the null pointer dereference analysis","0");
	knob_->RegisterStr("dangling_ptr","file name of the static information"
		" about the dangling pointer analysis","0");
	knob_->RegisterStr("buffer_overflow","file name of the static information"
		" about the buffer index","0");
}

void MemBug::Setup()
{
	InitializeNullPtrDeref();
	InitializeUnInitRead();
	InitializeDanglingPtr();
	InitializeBufferOverflow();
}

inline void MemBug::InitializeNullPtrDeref()
{
	null_ptr_deref_=new NullPtrDeref;
	std::string file_name=knob_->ValueStr("null_ptr_deref");
	if(!null_ptr_deref_->LoadStaticInfo(file_name.c_str())) {
		delete null_ptr_deref_;
		null_ptr_deref_=NULL;
	}
}

inline void MemBug::InitializeUnInitRead()
{
	uninit_read_=new UnInitRead;
	uninit_read_->CreateGlobalRegionFilter(new NullMutex);
}

inline void MemBug::InitializeDanglingPtr()
{
	dangling_ptr_=new DanglingPtr;
	std::string file_name=knob_->ValueStr("dangling_ptr");
	if(!dangling_ptr_->LoadStaticInfo(file_name.c_str())) {
		delete dangling_ptr_;
		dangling_ptr_=NULL;
	}
}

inline void MemBug::InitializeBufferOverflow()
{
	buffer_overflow_=new BufferOverflow;
	std::string file_name=knob_->ValueStr("buffer_overflow");
	if(!buffer_overflow_->LoadStaticInfo(file_name.c_str())) {
		delete buffer_overflow_;
		buffer_overflow_=NULL;
	}
}

thread_t MemBug::ProcessHarmfulRace(MemMeta *tmp_mem_meta,MemAccess *tmp_mem_acc1,
	MemAccess *tmp_mem_acc2)
{
	/* Null pointer dereference, uninitialized read and buffer overflow are both 
	   composed a read and a write access. 
	   Dangling pointer may be composed of two write accesses. */
	if((tmp_mem_acc1->type==RACE_EVENT_WRITE && tmp_mem_acc2->type==RACE_EVENT_READ) ||
		(tmp_mem_acc1->type==RACE_EVENT_READ && tmp_mem_acc2->type==RACE_EVENT_WRITE)) {
		// Null pointer dereference do not need meta
		if(null_ptr_deref_ && null_ptr_deref_->Harmful(tmp_mem_acc1,tmp_mem_acc2)) {
			return tmp_mem_acc1->type==RACE_EVENT_WRITE ? 
				tmp_mem_acc2->thd_id:tmp_mem_acc1->thd_id;
		}
		// Always make the read access go through laster.
		if(uninit_read_) {
			MemMeta *mem_meta=new MemMeta(tmp_mem_meta->addr);
			if(uninit_read_->Harmful(mem_meta,tmp_mem_acc1,tmp_mem_acc2))
				return tmp_mem_acc1->type==RACE_EVENT_WRITE ? 
					tmp_mem_acc2->thd_id:tmp_mem_acc1->thd_id;
		}
		// Always make the `delete` operator go through later. 
		if(dangling_ptr_) {
			return dangling_ptr_->Harmful(tmp_mem_acc1,tmp_mem_acc2);
		}
		// Always make the write changing the index go through later.
		if(buffer_overflow_ && buffer_overflow_->Harmful(tmp_mem_acc1,tmp_mem_acc2)) {
			return tmp_mem_acc1->type==RACE_EVENT_WRITE ? 
				tmp_mem_acc1->thd_id:tmp_mem_acc2->thd_id;
		}
	} else if(tmp_mem_acc1->type==RACE_EVENT_WRITE && 
		tmp_mem_acc2->type==RACE_EVENT_WRITE) {
		if(dangling_ptr_) {
			// Always make the `delete` operator go through later. 
			return dangling_ptr_->Harmful(tmp_mem_acc1,tmp_mem_acc2);
		}
	}
	return 0;
}
} //namespace race