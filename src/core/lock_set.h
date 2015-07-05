#ifndef __CORE_LOCK_SET_H
#define __CORE_LOCK_SET_H

#include <map>
#include "core/basictypes.h"
#include "core/atomic.h"

class LockSet {
public:
	typedef uint64 lock_version_t;
	static volatile uint32 ls_itsecs_;
	static volatile uint32 ls_assigns_;
	static volatile uint32 ls_adds_;
	static volatile uint32 ls_allocas_;

	LockSet() {ls_allocas_++;}
	~LockSet() {}

	bool Empty() {return set_.empty();}
	void Add(address_t addr) {set_[addr]=GetNextLockVersion();ls_adds_++;}
	void Remove(address_t addr) {
		if(set_.find(addr)!=set_.end())
			set_.erase(addr);
	}
	bool Exist(address_t addr) {return set_.find(addr)!=set_.end();}
	bool Exist(address_t addr,lock_version_t);
	void Clear(){set_.clear();}
	bool Match(LockSet *ls);
	bool Disjoint(LockSet *ls);
	bool Disjoint(LockSet *rmt_ls1,LockSet *rmt_ls2);
	std::string ToString();
	void IterBegin() {it_=set_.begin();}
	bool IterEnd() {return it_==set_.end();}
	void IterNext() {++it_;}
	address_t IterCurrAddr() {return it_->first;}
	lock_version_t IterCurrVersion() {return it_->second;}

	void Merge(LockSet *ls);
	void JoinAndMerge(LockSet *joint_ls,LockSet *ls);
	void Join(LockSet *joint_ls);

	bool SubLockSet(LockSet *ls);

	LockSet &operator =(const LockSet &ls) {
		set_=ls.set_;
		ls_assigns_++;
		return *this;
	}
	//simple counting
	uint32 GetMemSize() {
		return sizeof(LockVersionMap::value_type)*set_.size();
	}
protected:
	typedef std::map<address_t,lock_version_t> LockVersionMap;

	static lock_version_t GetNextLockVersion() {
		return ATOMIC_ADD_AND_FETCH(&currLockVersion,1);
	}

	LockVersionMap set_;
	LockVersionMap::iterator it_;

	static lock_version_t currLockVersion;

	//using default copy constructor
};

#endif /* __CORE_LOCK_SET_H */