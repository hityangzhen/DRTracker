#include "race/loop.h"
#include "core/log.h"

namespace race
{
LoopDB::LoopDB(RaceDB *race_db):race_db_(race_db)
{}

LoopDB::~LoopDB()
{
	//clear loop info
	for(LoopMap::iterator iter=loop_map_.begin();iter!=loop_map_.end();
		iter++) {
		std::set<Loop*> loop_set;
		for(LoopTable::iterator iiter=iter->second->begin();
			iiter!=iter->second->end();iiter++) {
			loop_set.insert(iiter->second);
		}
		for(std::set<Loop*>::iterator iiter=loop_set.begin();
			iiter!=loop_set.end();iiter++)
			delete *iiter;
		delete iter->second;
	}
	//clear the remainder
	for(SpinReadMetaTable::iterator iter=spin_rdmeta_table_.begin();
		iter!=spin_rdmeta_table_.end();iter++) {
		delete iter->second;
	}
}

thread_t LoopDB::GetSpinRelevantWriteThread(thread_t thd_id) 
{
	if(spin_rdmeta_table_.find(thd_id)==spin_rdmeta_table_.end())
		return 0;
	return spin_rdmeta_table_[thd_id]->spin_rlt_wrthd;
}

bool LoopDB::SpinReadThread(thread_t thd_id) 
{
	return spin_rdmeta_table_.find(thd_id)!=spin_rdmeta_table_.end();
}

void LoopDB::SetSpinReadThread(thread_t thd_id,Inst *inst) 
{
	if(spin_rdmeta_table_.find(thd_id)==spin_rdmeta_table_.end())
		spin_rdmeta_table_[thd_id]=new SpinReadMeta(inst);
	else
		spin_rdmeta_table_[thd_id]->spin_rdinst=inst;
}

 
void LoopDB::SetSpinRelevantWriteInst(thread_t thd_id,Inst *inst) 
{
	DEBUG_ASSERT(spin_rdmeta_table_[thd_id]);
	spin_rdmeta_table_[thd_id]->spin_rlt_wrinst=inst;
}
 
void LoopDB::SetSpinRelevantWriteThread(thread_t thd_id,thread_t wrthd_id) 
{
	DEBUG_ASSERT(spin_rdmeta_table_[thd_id]);
	spin_rdmeta_table_[thd_id]->spin_rlt_wrthd=wrthd_id;
}

uint32 LoopDB::GetSpinInnerCount(thread_t thd_id) 
{
	if(spin_rdmeta_table_.find(thd_id)==spin_rdmeta_table_.end())
		return 0;
	return spin_rdmeta_table_[thd_id]->spin_inner_count;
}

void LoopDB::ResetSpinInnerCount(thread_t thd_id) 
{
	DEBUG_ASSERT(spin_rdmeta_table_[thd_id]);
	spin_rdmeta_table_[thd_id]->spin_inner_count=0;
}

void LoopDB::LoadSpinReads(const char *file_name)
{
	std::fstream in(file_name,std::ios::in);
	if(!in) return ;
	const char *delimit=" ",*fn=NULL;
	const char *sl=NULL,*el=NULL,*ecl=NULL;
	char buffer[200];
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
		Loop *loop=new Loop(sl_int,el_int);

		//traverse all exiting condition lines
		while((ecl=strtok(NULL,delimit))) {
			//add the global ecl index
			exiting_cond_line_set_.insert(fn_str+ecl);
			//add the mapping of ecl to loop
			loop_map_[fn_str]->insert(std::make_pair(atoi(ecl),loop));
		}
	}
	in.close();
}

bool LoopDB::SpinRead(Inst *inst,RaceEventType type)
{ 
	if(type==RACE_EVENT_WRITE)
		return false;
	std::string file_name=inst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	std::stringstream ss; 
	ss<<file_name<<line;
	return exiting_cond_line_set_.find(ss.str())!=
		exiting_cond_line_set_.end();
}

bool LoopDB::SpinRead(std::string &file_name,int line,RaceEventType type)
{
	if(type==RACE_EVENT_WRITE)
		return false;
	std::stringstream ss;
	ss<<file_name<<line;
	return exiting_cond_line_set_.find(ss.str())!=
		exiting_cond_line_set_.end();
}

/**
 * if curr_inst is NULL, which indicates not to check if in the loop scope
 */
void LoopDB::ProcessWriteReadSync(thread_t curr_thd_id,Inst *curr_inst,
	VectorClock *wrthd_vc,VectorClock *curr_vc)
{
	if(spin_rdmeta_table_.find(curr_thd_id)==spin_rdmeta_table_.end())
		return ;
// INFO_FMT_PRINT("===============process write read sync:[%lx]===============\n",curr_thd_id);
	SpinReadMeta *rdmeta=spin_rdmeta_table_[curr_thd_id];
	DEBUG_ASSERT(rdmeta);
	Inst *lastest_rdinst=rdmeta->spin_rdinst;
	std::string file_name=lastest_rdinst->GetFileName();
	size_t found=file_name.find_last_of("/");
	file_name=file_name.substr(found+1);

	//lastest loop
	Loop *loop=(*loop_map_[file_name])[lastest_rdinst->GetLine()];
	DEBUG_ASSERT(loop);
	
	thread_t spin_rlt_wrthd=rdmeta->spin_rlt_wrthd;
	Inst *spin_rlt_wrinst=rdmeta->spin_rlt_wrinst;
	
	//if current inst still in loop region
	if(spin_rlt_wrthd!=0 && (!curr_inst ||
		!loop->InLoop(curr_inst->GetLine()))) {
		//construct write->read sync
		curr_vc->Join(wrthd_vc);
		wrthd_vc->Increment(spin_rlt_wrthd);
		if(curr_inst)
INFO_FMT_PRINT("+++++++++++++++write->read sync,curr inst:[%s]++++++++++++++\n",curr_inst->ToString().c_str());
		//set the race constructed by exiting condition and lastest 
		//write inst pair to be BENIGN
		std::map<Race*,RaceEvent*> result; //race and corresponding read event
		race_db_->FindRacesByOneSide(spin_rlt_wrthd,spin_rlt_wrinst,
			RACE_EVENT_WRITE,result,false);
		for(std::map<Race*,RaceEvent*>::iterator iter=result.begin();
			iter!=result.end();iter++) {
			RaceEvent *race_event=iter->second;
			//find the corresponding spin read event
			if(race_event->type()==RACE_EVENT_READ && 
				loop->InLoop(race_event->inst()->GetLine())) {
INFO_FMT_PRINT("++++++++++++++++benign race, read inst:[%s]++++++++++++++++\n",race_event->inst()->ToString().c_str());
				iter->first->set_status(Race::BENIGN);
			}
		}
		delete rdmeta;
		spin_rdmeta_table_.erase(curr_thd_id);
	} else {
		//postpone confirming if exting condition satisfied
		rdmeta->spin_inner_count++;
	}
// INFO_PRINT("==============process write read sync end=============\n");
}

}//namespace