#include "race/pre_group.h"
#include "core/log.h"

namespace race
{

PreGroup::PreGroup()
{}

PreGroup::~PreGroup()
{}

void PreGroup::Register()
{
	knob_->RegisterInt("unit_size_","the monitoring granularity in bytes","4");
	knob_->RegisterBool("enable_pre_group","whether enable the pre_group","0");
	knob_->RegisterStr("sorted_static_profile","output file of sorted pstmt pairs",
		"0");
}
bool PreGroup::Enabled()
{
	return knob_->ValueBool("enable_pre_group");
}
void PreGroup::Setup(Mutex *lock,PRaceDB *prace_db)
{
	internal_lock_=lock;
	prace_db_=prace_db;
	unit_size_=knob_->ValueInt("unit_size_");
	filter_=new RegionFilter(internal_lock_->Clone());
	//set analyzer descriptor
	desc_.SetHookBeforeMem();
	desc_.SetHookPthreadFunc();
	desc_.SetHookMallocFunc();
	desc_.SetHookAtomicInst();
}

void PreGroup::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	Detector::ThreadStart(curr_thd_id,parent_thd_id);
	thd_ec_map_[curr_thd_id]=1;
}

void PreGroup::BeforeMemRead(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr, size_t size)
{
	ScopedLock lock(internal_lock_);
	RecordPStmtLogicalTime(curr_thd_id,inst);
}
void PreGroup::BeforeMemWrite(thread_t curr_thd_id, timestamp_t curr_thd_clk,
	Inst *inst, address_t addr, size_t size)
{
	ScopedLock lock(internal_lock_);
	RecordPStmtLogicalTime(curr_thd_id,inst);
}

void PreGroup::BeforeAtomicInst(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,std::string type, address_t addr)
{
	ScopedLock lock(internal_lock_);
	RecordPStmtLogicalTime(curr_thd_id,inst);
}

inline void PreGroup::RecordPStmtLogicalTime(thread_t curr_thd_id,Inst *inst)
{
	std::string file_name=inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	PStmt *pstmt=prace_db_->GetPStmt(file_name,line);
	SortedPStmt *sorted_pstmt=dynamic_cast<SortedPStmt*>(pstmt);
	if(sorted_pstmt==NULL)
		return ;
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	VectorClock &vc=sorted_pstmt->GetVectorClock();
	//first access the stmt or dfferent vector clock 
	if(!vc.Equal(curr_vc)) {
		sorted_pstmt->SetVectorClock(*curr_vc_map_[curr_thd_id]);
		sorted_pstmt->SetExecCount(thd_ec_map_[curr_thd_id]++);
	}
}

void PreGroup::Export()
{
	std::string file_name=knob_->ValueStr("sorted_static_profile");
	if(file_name.compare("0")==0)
		return ;
	prace_db_->Export(file_name.c_str());
}

}//namespace race