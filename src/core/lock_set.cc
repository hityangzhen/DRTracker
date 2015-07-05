#include "core/lock_set.h"
#include "core/log.h"
#include <sstream>


LockSet::lock_version_t LockSet::currLockVersion=0;
volatile uint32 LockSet::ls_itsecs_=0;
volatile uint32 LockSet::ls_assigns_=0;
volatile uint32 LockSet::ls_adds_=0;
volatile uint32 LockSet::ls_allocas_=0;

bool LockSet::Exist(address_t addr,lock_version_t version)
{
	LockVersionMap::iterator it=set_.find(addr);
	if(it!=set_.end() && it->second==version) 
		return true;
	return false;
}


bool LockSet::Match(LockSet *ls)
{
	if(set_.size()!=ls->set_.size())
		return false;
	for(LockVersionMap::iterator it=set_.begin();it!=set_.end();++it) {
		LockVersionMap::iterator mit=ls->set_.find(it->first);
		if(mit==ls->set_.end())
			return false;
	}
	return true;
}

bool LockSet::Disjoint(LockSet *ls)
{
	ls_itsecs_++;
	if(ls==NULL)
		return true;
	for(LockVersionMap::iterator it=set_.begin();it!=set_.end();++it) {
		LockVersionMap::iterator mit=ls->set_.find(it->first);
		if(mit!=ls->set_.end())
			return false;
	}
	return true;
}

bool LockSet::Disjoint(LockSet *rmt_ls1,LockSet *rmt_ls2) 
{
	ls_itsecs_++;
	if(rmt_ls1==NULL || rmt_ls2==NULL)
		return true;
	for(LockVersionMap::iterator it=set_.begin();it!=set_.end();it++) {
		LockVersionMap::iterator mit1=rmt_ls1->set_.find(it->first);
		LockVersionMap::iterator mit2=rmt_ls2->set_.find(it->first);
		if(mit1!=rmt_ls1->set_.end() && mit2!=rmt_ls2->set_.end()) {
			//compare the version number between two remote lock sets
			if(mit1->second==mit2->second)
				return false;
		}
	}
	return true;
}


//intersection
void LockSet::Merge(LockSet *ls)
{
	ls_itsecs_++;
	if(ls==NULL)
		set_.clear();
	for(LockVersionMap::iterator iter=set_.begin();iter!=set_.end();) {
		LockVersionMap::iterator mit=ls->set_.find(iter->first);
		if(mit==ls->set_.end()) {
			//map container eraser
			set_.erase(iter++);
		}
		else
			iter++;
	}
}


bool LockSet::SubLockSet(LockSet *ls)
{
	if(ls==NULL)
		return false;
	for(LockVersionMap::iterator iter=set_.begin();iter!=set_.end();
		iter++) {
		if(ls->set_.find(iter->first)==ls->set_.end())
			return false;
	}
	return true;
}

void LockSet::JoinAndMerge(LockSet *joint_ls,LockSet *ls)
{
	//join
	Join(joint_ls);
	//merge
	Merge(ls);
}

void LockSet::Join(LockSet *joint_ls)
{
	if(joint_ls==NULL)
		return ;
	for(LockVersionMap::iterator iter=joint_ls->set_.begin();
		iter!=joint_ls->set_.end();iter++) {
		if(set_.find(iter->first)==set_.end())
			set_.insert(*iter);
	}
}

std::string LockSet::ToString()
{
	std::stringstream ss;
	ss<<"[";
	for(LockVersionMap::iterator it=set_.begin();it!=set_.end();++it)
		ss<<std::hex<<"0x"<<it->first<<" ";
	ss<<"]";
	return ss.str();
}