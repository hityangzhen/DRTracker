#ifndef _RACE_POTENTIAL_RACE_H
#define _RACE_POTENTIAL_RACE_H

#include "core/basictypes.h"
#include <tr1/unordered_map>
#include <set>
#include "core/log.h"
#include <sstream>
#include <tr1/unordered_set>
#include "core/vector_clock.h"

namespace race
{
// single potential race info
class PStmt
{
public:
	PStmt(std::string fn,int l):file_name_(fn),line_(l) {}
	PStmt(const char *fn,const char *l):file_name_(fn),line_(atoi(l)) {}
	virtual ~PStmt() {}
	std::string file_name_;
	int line_;
};

class SortedPStmt:public PStmt {
public:
	SortedPStmt(std::string fn,int l): PStmt(fn,l) {
		exec_count_=0;
	}
	SortedPStmt(const char *fn,const char *l): PStmt(fn,l) {
		exec_count_=0;
	}
	~SortedPStmt() {}
	void JoinVectorClock(VectorClock *vc) { vc_.Join(vc); }
	void SetExecCount(uint64 ec) { exec_count_=ec; }
	uint64 GetExecCount() { return exec_count_; }
	VectorClock &GetVectorClock() { return vc_; }
	VectorClock vc_;
	uint64 exec_count_;
};

class PRaceDB {
public:
	typedef std::tr1::unordered_map<uint64,PStmt *> PStmtSet;
	typedef std::tr1::unordered_set<PStmt *> PRlvStmtSet;
	typedef std::tr1::unordered_map<PStmt *, PRlvStmtSet *> PStmtMap;
	PRaceDB() {}
	~PRaceDB() {
		for(PStmtMap::iterator iter=pstmt_map_.begin();iter!=pstmt_map_.end();
			iter++) {
			if(iter->second)
				delete iter->second;
		}

		for(PStmtSet::iterator iter=pstmt_set_.begin();iter!=pstmt_set_.end();
			iter++) {
			if(iter->second)
				delete iter->second;
		}
	}
	//for find pstmt
	PStmt *GetPStmt(std::string &fn,int l) {
		uint64 key=FilenameAndLineHash(fn,l);
		if(pstmt_set_.find(key)!=pstmt_set_.end())
			return pstmt_set_[key];
		return NULL;
	}
	PStmt *GetPStmt(const char *fn,int l) {
		uint64 key=FilenameAndLineHash(fn,l);
		if(pstmt_set_.find(key)!=pstmt_set_.end())
			return pstmt_set_[key];
		return NULL;
	}
	//for load pstmt
	PStmt *GetPStmt(const char *fn,const char *l) {
		uint64 key=FilenameAndLineHash(fn,atoi(l));
		if(pstmt_set_.find(key)!=pstmt_set_.end())
			return pstmt_set_[key];
		return NULL;	
	}

	PStmt *GetPStmtAndCreate(const char *fn,const char *l) {
		uint64 key=FilenameAndLineHash(fn,atoi(l));
		if(pstmt_set_.find(key)==pstmt_set_.end())
			pstmt_set_[key]=new PStmt(fn,l);
		return pstmt_set_[key];
	}

	PStmt *GetSortedPStmtAndCreate(const char *fn,const char *l) {
		uint64 key=FilenameAndLineHash(fn,atoi(l));
		if(pstmt_set_.find(key)==pstmt_set_.end())
			pstmt_set_[key]=new SortedPStmt(fn,l);
		return pstmt_set_[key];
	}	

	void BuildRelationMapping(PStmt *first_pstmt,const char *fn,const char *l) {
		SortedPStmt *sorted_pstmt=dynamic_cast<SortedPStmt*>(first_pstmt);
		PStmt *second_pstmt=NULL;
		if(sorted_pstmt)
			second_pstmt=GetSortedPStmtAndCreate(fn,l);
		else
			second_pstmt=GetPStmtAndCreate(fn,l);
		//build the double-side relation
		if(pstmt_map_.find(first_pstmt)==pstmt_map_.end())
			pstmt_map_[first_pstmt]=new PRlvStmtSet;
		pstmt_map_[first_pstmt]->insert(second_pstmt);
		if(pstmt_map_.find(second_pstmt)==pstmt_map_.end())
			pstmt_map_[second_pstmt]=new PRlvStmtSet;
		pstmt_map_[second_pstmt]->insert(first_pstmt);
	}

	bool SecondPotentialStatement(PStmt *first_pstmt,PStmt *second_pstmt) {
		if(pstmt_map_.find(first_pstmt)==pstmt_map_.end())
			return false;
		PRlvStmtSet *prlvstmt_set=pstmt_map_[first_pstmt];
		return prlvstmt_set->find(second_pstmt)!=prlvstmt_set->end();
	}

	void RemoveRelationMapping(PStmt *first_pstmt,PStmt *second_pstmt) {
		if(pstmt_map_.find(first_pstmt)==pstmt_map_.end() ||
			pstmt_map_.find(second_pstmt)==pstmt_map_.end())
			return ;
		pstmt_map_[first_pstmt]->erase(second_pstmt);
		if(pstmt_map_[first_pstmt]->empty()) {
			delete pstmt_map_[first_pstmt];
			pstmt_map_.erase(first_pstmt);
		}
		//if first pstmt is equal second pstmt
		if(first_pstmt==second_pstmt)
			return ;
		pstmt_map_[second_pstmt]->erase(first_pstmt);
		if(pstmt_map_[second_pstmt]->empty()) {
			delete pstmt_map_[second_pstmt];
			pstmt_map_.erase(second_pstmt);
		}
	}

	bool HasFullyVerified(PStmt *first_pstmt) {
		return pstmt_map_.find(first_pstmt)==pstmt_map_.end() ;
	}

	void Export(const char *fn) {
		std::fstream out(fn,std::fstream::out | std::fstream::trunc);
		if(!out)
			return ;
		for(PStmtMap::iterator iter=pstmt_map_.begin();iter!=pstmt_map_.end();
			iter++) {
			std::stringstream ss;
			SortedPStmt *sorted_pstmt1=dynamic_cast<SortedPStmt*>(iter->first);
			if(sorted_pstmt1==NULL)
				continue;
			for(PRlvStmtSet::iterator iiter=iter->second->begin();
				iiter!=iter->second->end();) {
				SortedPStmt *sorted_pstmt2=dynamic_cast<SortedPStmt*>(*iiter);
				if(sorted_pstmt2==NULL) {
					iiter++;
					continue;				
				}
				ss<<sorted_pstmt1->file_name_<<" "
					<<sorted_pstmt1->line_<<" "
					<<sorted_pstmt1->vc_.OutputString()<<" "
					<<sorted_pstmt1->exec_count_<<" "
					<<sorted_pstmt2->file_name_<<" "
					<<sorted_pstmt2->line_<<" "
					<<sorted_pstmt2->vc_.OutputString()<<" "
					<<sorted_pstmt2->exec_count_<<std::endl;
				//different pstmt
				if(sorted_pstmt1!=sorted_pstmt2) {
					//remove a pair after export
					pstmt_map_[(PStmt*)sorted_pstmt2]->erase((PStmt*)sorted_pstmt1);
				}
				// unordered_set erase is likely as the ordered container
				iiter=iter->second->erase(iiter);
			}
			out<<ss.str();
		}
		out.close();
	}
private:

	uint64 FilenameAndLineHash(std::string &file_name,int line) {
		uint64 key=0;
		for(size_t i=0;i<file_name.size();i++)
			key += file_name[i];
		key += line;
		return key;
	}
	uint64 FilenameAndLineHash(const char *file_name,int line) {
		uint64 key=0;
		const char *ch=file_name;
		while(*ch)
			key += *ch++;
		key += line;
		return key;
	}

	PStmtMap pstmt_map_;
	PStmtSet pstmt_set_;
};

} //namespace race

#endif // _RACE_POTENTIAL_RACE_H
