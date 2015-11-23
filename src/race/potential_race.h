#ifndef _RACE_POTENTIAL_RACE_H
#define _RACE_POTENTIAL_RACE_H

#include "core/basictypes.h"
#include <tr1/unordered_map>
#include <set>
#include "core/log.h"
#include <sstream>
#include <tr1/unordered_set>

namespace race
{


// single potential race info
struct PStmt
{
	PStmt(std::string fn,int l):file_name_(fn),line_(l)
	{}

	PStmt(const char *fn,const char *l):file_name_(fn),line_(atoi(l))
	{}

	std::string file_name_;
	int line_;
};

class PRaceDB {
public:
	typedef std::tr1::unordered_map<std::string,PStmt *> PStmtSet;
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

	PStmt *GetPStmt(std::string &fn,int l) {
		std::stringstream ss;
		ss<<l;
		std::string str(fn+ss.str());
		if(pstmt_set_.find(str)!=pstmt_set_.end())
			return pstmt_set_[str];
		return NULL;
	}

	PStmt *GetPStmt(const char *fn,const char *l) {
		std::string str=std::string(fn)+l;
		if(pstmt_set_.find(str)!=pstmt_set_.end())
			return pstmt_set_[str];
		return NULL;	
	}

	PStmt *GetPStmtAndCreate(const char *fn,const char *l) {
		std::string str=std::string(fn)+l;
		if(pstmt_set_.find(str)==pstmt_set_.end())
			pstmt_set_[str]=new PStmt(fn,l);
		return pstmt_set_[str];
	}

	void BuildRelationMapping(PStmt *first_pstmt,const char *fn,const char *l) {
		PStmt *second_pstmt=GetPStmtAndCreate(fn,l);
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
		pstmt_map_[first_pstmt]->erase(second_pstmt);
		if(pstmt_map_[first_pstmt]->empty()) {
			delete pstmt_map_[first_pstmt];
			pstmt_map_.erase(first_pstmt);
		}
		pstmt_map_[second_pstmt]->erase(first_pstmt);
		if(pstmt_map_[second_pstmt]->empty()) {
			delete pstmt_map_[second_pstmt];
			pstmt_map_.erase(second_pstmt);
		}
	}

private:
	PStmtMap pstmt_map_;
	PStmtSet pstmt_set_;
};

} //namespace race

#endif // _RACE_POTENTIAL_RACE_H
