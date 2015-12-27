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
	bool InLoop(int line) {
		return line>=start_line_ && line<=end_line_;
	}
protected:
	int start_line_;
	int end_line_;
};

//extend loop with the exiting conditions
class XLoop:public Loop{
public:
	XLoop(int sl,int el):Loop(sl,el) {}
	~XLoop() {}
	void AddExitingCondLine(int line) {
		exiting_cond_line_set_.insert(line);
	}
	bool ExitingCondLine(int line) {
		return exiting_cond_line_set_.find(line)!=exiting_cond_line_set_.end();
	}
protected:
	std::tr1::unordered_set<int> exiting_cond_line_set_;
};

class LoopDB {
protected:
	class SpinReadMeta {
	public:
		SpinReadMeta(Inst *inst):spin_inner_count(0),spin_rdinst(inst),
			spin_rlt_wrinst(NULL),spin_rlt_wrthd(0) {}
		~SpinReadMeta() {}
		//spinning loop execution count
		uint32 spin_inner_count;
		//spinning loop lastest existing condition read inst
		Inst *spin_rdinst;
		//spinning loop relevant lastest write inst
		Inst *spin_rlt_wrinst;
		//spinning loop relevant lastest write thread
		thread_t spin_rlt_wrthd;
	};
public:
	typedef std::tr1::unordered_map<int,Loop *> LoopTable;
	typedef std::tr1::unordered_map<std::string,LoopTable *> LoopMap;
	typedef std::tr1::unordered_set<std::string> ExitingCondLineSet;
	typedef std::tr1::unordered_map<thread_t,SpinReadMeta *> SpinReadMetaTable;
	typedef std::set<thread_t> SpinThreadSet;
	LoopDB(RaceDB *race_db);
	~LoopDB();
	//ad-hoc
	void LoadSpinReads(const char *file_name);
	bool SpinRead(Inst *inst,RaceEventType type);
	bool SpinRead(std::string &file_name,int line,RaceEventType type);
	void ProcessWriteReadSync(thread_t curr_thd_id,Inst *curr_inst,
		VectorClock *wrthd_vc,VectorClock *curr_vc);
	uint32 GetSpinInnerCount(thread_t thd_id);
	void ResetSpinInnerCount(thread_t thd_id);	
	bool SpinReadThread(thread_t thd_id);
	void SetSpinReadThread(thread_t thd_id,Inst *inst);
	void SetSpinRelevantWriteInst(thread_t thd_id,Inst *inst);	
	void SetSpinRelevantWriteThread(thread_t thd_id,thread_t wrthd_id);
	thread_t GetSpinRelevantWriteThread(thread_t thd_id);
protected:
	RaceDB *race_db_;
	//loops relevant info
	LoopMap loop_map_;
	ExitingCondLineSet exiting_cond_line_set_;
	//spinning loop thread and meta mapping
	SpinReadMetaTable spin_rdmeta_table_;
private:
	DISALLOW_COPY_CONSTRUCTORS(LoopDB);
};

}
#endif //__RACE_LOOP_H