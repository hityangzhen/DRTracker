/**
  * @file race/membug.h
  * Define the concurrent memory bug.
  * 
  * Now, we are enabled to analyze four types concurrent memory bugs
  * that can result in the program crash.
  * 
  * Null pointer deference
  * Uninitialized read
  * Dangling pointer
  * Buffer overflow
  */
#ifndef __RACE_MEMBUG_H
#define __RACE_MEMBUG_H

#include "core/basictypes.h"
#include <map>
#include <vector>
#include <tr1/unordered_set>
#include "race/race.h"
#include "core/filter.h"
#include "core/sync.h"
#include "core/knob.h"

namespace race
{

// The abstract data for the memory meta info.
class MemMeta {
public:
	typedef std::tr1::unordered_map<address_t,MemMeta *> MemTable;
	explicit MemMeta(address_t a):addr(a) {}
	~MemMeta() {}
	address_t addr;
};
// The abstract data for the memory access info.
class MemAccess {
public:
	MemAccess(thread_t tid,Inst *i,RaceEventType t):thd_id(tid),inst(i),type(t){}
	~MemAccess() {}
	thread_t thd_id;
	Inst *inst;
	RaceEventType type;
};

/**
  * Null pointer dereference concurrent memory bug is composed of
  * a wp and rp from the two different threads. wp writes a NULL
  * to a shared pointer. rp reads pointer and conducts the pointer
  * deference. 
  */
class NullPtrDeref {
public:
	NullPtrDeref();
	~NullPtrDeref();
	//
	bool Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2);
	// Load the information about the null pointer read
	bool LoadStaticInfo(const char *file_name);
protected:
	typedef std::tr1::unordered_set<uint64> NullPtrSet;
	// Write the NULL to the pointer or read the pointer by dereference
	bool ValidPtrReadOrWrite(MemAccess *mem_acc);
	/* Pointer read access within the condition expression. Like
	   `if(ptr==NULL)` or `if(ptr)` or `if(!ptr)` etc. */
	NullPtrSet null_ptrrd_set_; 
	/* Pointer write access must contains NULL. Like 
	   `ptr=NULL` etc. */
	NullPtrSet null_ptrwr_set_;
private:
	DISALLOW_COPY_CONSTRUCTORS(NullPtrDeref);
};

/**
  * Un-initialized memory read bug is composed of a read and a definition
  * to the shared memory location.
  */
class UnInitRead {
public:
	UnInitRead();
	~UnInitRead();
	//
	bool Harmful(MemMeta *meta,MemAccess *mem_acc1,MemAccess *mem_acc2);
	void CreateGlobalRegionFilter(Mutex *lock);
	void AddGlobalRegion(address_t region_start,size_t region_size);
protected:
	typedef std::map<thread_t,bool> ThreadFlagMap;
	typedef std::map<MemMeta*,ThreadFlagMap *> MemMetaInitTable;
	bool InGlobalRegion(address_t addr);
	// Set the first write to the heap memory location
	void SetInitWrite(MemMeta *meta,thread_t thd_id);
	// If the thread has been write initially
	bool HasWrite(MemMeta *meta,thread_t thd_id);
	RegionFilter *filter_;
	MemMetaInitTable memmeta_init_table_;
private:
	DISALLOW_COPY_CONSTRUCTORS(UnInitRead);
};

/**
  * Danlging pointer access bug is composed of an access to the heap memory
  * location(write or read) and a delete operation to the heap memory.
  * Danlging pointer access race is mainly incurred by the vptr in C++.
  * When using the `delete` operator will clear the vptr and when accessing
  * virtual function will access the vptr.
  * In the C language program, `free` system function may not be access the
  * shared memory location and may not conduct the data race.
  */
class DanglingPtr {
public:
	DanglingPtr();
	~DanglingPtr();

	// Return the thread executes the `delete` operator.
	thread_t Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2);
	void CreateFreedRegionFilter(Mutex *lock);
	void AddFreedRegion(address_t region_start,size_t region_size);
	// Load the information about the `delete` operator.
	bool LoadStaticInfo(const char *file_name);
protected:
	typedef std::tr1::unordered_set<uint64> DeletePtrSet;
	bool InFreedRegion(address_t addr);
	bool DeletePtr(MemAccess *mem_acc);
	RegionFilter *filter_;
	DeletePtrSet del_ptr_set_;
private:
	DISALLOW_COPY_CONSTRUCTORS(DanglingPtr);
};

/**
  * Buffer overflow memory bug is composed of a write access and a read
  * access to the shared index variable. 
  * We now only consider the shared memory buffer.
  */
class BufferOverflow {
public:
	BufferOverflow();
	~BufferOverflow();
	
	bool Harmful(MemAccess *mem_acc1,MemAccess *mem_acc2);
	// Load the information about the buffer index.
	bool LoadStaticInfo(const char *file_name);
protected:
	typedef std::tr1::unordered_set<uint64> BufferIndexSet;
	bool BufferIndex(MemAccess *mem_acc);
	BufferIndexSet buf_idx_set_;
private:
	DISALLOW_COPY_CONSTRUCTORS(BufferOverflow);
};
/**
  * Manage all types of memory bugs
  */
class MemBug {
public:
	MemBug(Knob *knob);
	~MemBug();

	void Register();
	void Setup();
	MemMeta CreateMemMeta(address_t addr) {
		return MemMeta(addr);
	}
	MemAccess CreateMemAccess(thread_t tid,Inst *i,RaceEventType t) {
		return MemAccess(tid,i,t);
	};
	/* Process different type memory bugs and return the harmful thread
	   that should go through later. */
	thread_t ProcessHarmfulRace(MemMeta *tmp_mem_meta,MemAccess *tmp_mem_acc1,
		MemAccess *tmp_mem_acc2);
	// Wrapper function of the `AddGlobalRegion` from the UnInitRead
	void AddGlobalRegion(address_t region_start,size_t region_size) {
		if(uninit_read_)
			uninit_read_->AddGlobalRegion(region_start,region_size);
	}

protected:
	void InitializeNullPtrDeref();
	void InitializeUnInitRead();
	void InitializeDanglingPtr();
	void InitializeBufferOverflow();
	Knob *knob_;

	NullPtrDeref *null_ptr_deref_;
	UnInitRead *uninit_read_;
	DanglingPtr *dangling_ptr_;
	BufferOverflow *buffer_overflow_;
private:
	DISALLOW_COPY_CONSTRUCTORS(MemBug);
};

} // namespace race

#endif // __RACE_MEMBUG_H