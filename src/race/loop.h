#ifndef __RACE_LOOP_H
#define __RACE_LOOP_H

#include <tr1/unordered_set>
#include <tr1/unordered_map>
#include <set>
#include <map>
#include "core/basictypes.h"
#include "race/race.h"
#include "core/vector_clock.h"

// for spinning read loop analysis

namespace race 
{
#define SPIN_READ_COUNT 6

class Loop {
public:
	Loop(int sl,int el):start_line_(sl),end_line_(el) {}
	~Loop() {}
	void SetProcedureScope(int psl,int pel) {
		procedure_start_line_=psl;
		procedure_end_line_=pel;
	}
	bool InLoop(int line) {
		return line>=start_line_ && line<=end_line_;
	}
	bool OutLoopInProcedure(int line) {
		return line>end_line_ && line<=procedure_end_line_;
	}
protected:
	int start_line_; //loop start line
	int end_line_; //loop end line
	int procedure_start_line_; //procedure start line
	int procedure_end_line_; //procedure end line
};

class LoopDB {
protected:
	class SpinReadMeta {
	public:
		SpinReadMeta(Inst *inst):spin_inner_count(0),spin_rdinst(inst),
			spin_rlt_wrinst(NULL),spin_rlt_wrthd(0),spin_callinst(NULL),
			spin_rlt_wrlocked(false) {}
		~SpinReadMeta() {}
		//spinning loop execution count
		uint32 spin_inner_count;
		//spinning loop lastest existing condition read inst
		Inst *spin_rdinst;
		//spinning loop relevant lastest write inst
		Inst *spin_rlt_wrinst;
		//spinning loop relevant lastest write thread
		thread_t spin_rlt_wrthd;
		//
		address_t spin_rdaddr;
		//spinning read called function inst
		Inst *spin_callinst;
		//if spinning loop relevant lastest write protected by common lock
		bool spin_rlt_wrlocked;
	};
	class SpinReadCalledFuncMeta {
	public:
		SpinReadCalledFuncMeta(Inst *i,bool f):inst(i),flag(f) {}
		~SpinReadCalledFuncMeta() {}
		Inst *inst;
		bool flag;
	};
public:
	typedef std::tr1::unordered_map<int,Loop *> LoopTable;
	typedef std::tr1::unordered_map<std::string,LoopTable *> LoopMap;
	typedef std::tr1::unordered_set<uint64> ExitingCondLineSet;
	typedef std::tr1::unordered_map<thread_t,SpinReadMeta *> SpinReadMetaTable;
	typedef std::set<thread_t> SpinThreadSet;
	typedef std::map<thread_t,SpinReadCalledFuncMeta *> ThreadSpinReadCFMetaMap;
	typedef std::tr1::unordered_set<uint64> ExitingNodeFirstLineSet;

	LoopDB(RaceDB *race_db);
	~LoopDB();
	//ad-hoc
	bool LoadSpinReads(const char *file_name);
	bool SpinRead(Inst *inst,RaceEventType type);
	bool SpinRead(std::string &file_name,int line,RaceEventType type);
	bool ExitingNodeFirstLine(Inst *inst);
	bool ExitingNodeFirstLine(std::string &file_name,int line);
	bool SpinReadCalledFunc(Inst *inst);
	void ProcessWriteReadSync(thread_t curr_thd_id,Inst *curr_inst,
		VectorClock *wrthd_vc,VectorClock *curr_vc);
	uint32 GetSpinInnerCount(thread_t thd_id);
	void ResetSpinInnerCount(thread_t thd_id);	
	bool SpinReadThread(thread_t thd_id);
	void SetSpinReadThread(thread_t thd_id,Inst *inst);
	void RemoveSpinReadMeta(thread_t thd_id);
	void SetSpinRelevantWriteLocked(thread_t thd_id,bool locked);
	void SetSpinReadAddr(thread_t thd_id,address_t addr);
	void SetSpinReadCallInst(thread_t thd_id);
	address_t GetSpinReadAddr(thread_t thd_id);
	void SetSpinRelevantWriteInst(thread_t thd_id,Inst *inst);	
	void SetSpinRelevantWriteThread(thread_t thd_id,thread_t wrthd_id);
	void SetSpinRelevantWriteThreadAndInst(thread_t thd_id,thread_t wrthd_id,
		Inst *wrinst);
	thread_t GetSpinRelevantWriteThread(thread_t thd_id);
	void SetSpinReadCalledFunc(thread_t curr_thd_id,Inst *inst,bool flag);
	void RemoveSpinReadCalledFunc(thread_t curr_thd_id);
	bool SpinReadCalledFuncThread(thread_t curr_thd_id);
	uint64 FilenameAndLineHash(std::string &file_name,int line) {
		uint64 key=0;
		for(size_t i=0;i<file_name.size();i++)
			key += file_name[i];
		key += line;
		return key;
	}
protected:
	RaceDB *race_db_;
	//loops relevant info
	LoopMap loop_map_;
	ExitingCondLineSet exiting_cond_line_set_;
	//spinning loop thread and meta mapping
	SpinReadMetaTable spin_rdmeta_table_;
	//spinning loop thread  and called function mapping
	ThreadSpinReadCFMetaMap spinthd_cfmeta_map_;
	//first line of the exiting node(contains `break` or `return` or `goto`)
	ExitingNodeFirstLineSet en_first_line_set_;
private:
	DISALLOW_COPY_CONSTRUCTORS(LoopDB);
};

}
#endif //__RACE_LOOP_H