#include "race/cond_wait.h"
#include "core/log.h"

namespace race
{
CondWaitDB::CondWaitDB()
{}

CondWaitDB::~CondWaitDB()
{
	//clear loop info
	for(CondWaitLoopMap::iterator iter=cwloop_map_.begin();
		iter!=cwloop_map_.end();iter++) {
		std::set<Loop*> loop_set;
		for(CondWaitLoopTable::iterator iiter=iter->second->begin();
			iiter!=iter->second->end();iiter++) {
			loop_set.insert(iiter->second);
		}
		for(std::set<Loop*>::iterator iiter=loop_set.begin();
			iiter!=loop_set.end();iiter++)
			delete *iiter;
		delete iter->second;
	}
	//clear meta
	for(ThreadLSMetasMap::iterator iter=thd_lsmetas_map_.begin();
		iter!=thd_lsmetas_map_.end();iter++) {
		LockSignalMetaDeque *lsmetas=iter->second;
		for(LockSignalMetaDeque::iterator iiter=lsmetas->begin();
			iiter!=lsmetas->end();iiter++) {
			delete *iiter;
		}
		delete lsmetas;
	}
}

void CondWaitDB::AddLockSignalWriteMeta(thread_t thd_id,VectorClock &vc,
	address_t addr,address_t lk_addr)
{
	if(thd_lsmetas_map_.find(thd_id)==thd_lsmetas_map_.end())
		thd_lsmetas_map_[thd_id]=new LockSignalMetaDeque;
	LockSignalMetaDeque *lsmetas=thd_lsmetas_map_[thd_id];
	if(lsmetas->empty()) {
		LockSignalMeta *lsmeta=new LockSignalMeta(vc);
		//create current lock corresponding write meta set
		LockWritesMeta *lk_wrs_meta=new LockWritesMeta(lk_addr);
		//add to the write meta set
		lk_wrs_meta->AddWriteMeta(new WriteMeta(addr));
		lsmeta->AddLockWritesMeta(lk_wrs_meta);

		lsmetas->push_back(lsmeta);
	}else {
		LockSignalMeta *lsmeta=lsmetas->back();
		if(lsmeta->vc.Equal(&vc)) {
			LockWritesMeta *lk_wrs_meta=lsmeta->GetLastestLockWritesMeta();
			//same protecting lock, only need to add the write meta
			if(lk_wrs_meta->lock_addr==lk_addr) {
				lk_wrs_meta->AddWriteMeta(new WriteMeta(addr));
			//different protecting lock, need to create lock writes meta
			//and then add the write meta
			}else {
				lk_wrs_meta=new LockWritesMeta(lk_addr);
				lk_wrs_meta->AddWriteMeta(new WriteMeta(addr));
				lsmeta->AddLockWritesMeta(lk_wrs_meta);
			}
		}
		else {
			//we ignore the write in signal unlock scope
			if(lsmeta->signal_unlock)
				return ;
			// INFO_FMT_PRINT("==========add lock signal write meta==========\n");
			//equal or smaller
			DEBUG_ASSERT(lsmeta->vc.HappensBefore(&vc)==true);
			//pop back and free
			if(lsmeta->expired) {
				lsmetas->pop_back();
				delete lsmeta;
			}
			lsmeta=new LockSignalMeta(vc);
			LockWritesMeta *lk_wrs_meta=new LockWritesMeta(lk_addr);
			lk_wrs_meta->AddWriteMeta(new WriteMeta(addr));
			lsmeta->AddLockWritesMeta(lk_wrs_meta);
			lsmetas->push_back(lsmeta);
		}
	}
}

CondWaitDB::LockSignalMeta *CondWaitDB::CorrespondingLockSignalMeta(
	VectorClock &vc,address_t addr,address_t lk_addr)
{
	for(ThreadLSMetasMap::iterator iter=thd_lsmetas_map_.begin();
		iter!=thd_lsmetas_map_.end();iter++) {
		LockSignalMetaDeque *lsmetas=iter->second;
		for(LockSignalMetaDeque::iterator iiter=lsmetas->begin();
			iiter!=lsmetas->end();) {
			LockSignalMeta *lsmeta=*iiter;
			//clear the expired lock signal meta
			if(lsmeta->expired) {
				iiter=lsmetas->erase(iiter);
				delete lsmeta;
				continue ;
			}
			//filter the unactived lock signal meta
			if(!lsmeta->actived) {
				iiter++;
				continue;
			}
			//filter the synchronized 
			if(lsmeta->vc.HappensBefore(&vc)) {
				iiter++;
				continue;
			}
			//traverse all lock writes meta
			LockWritesMeta *lk_wrs_meta=NULL;
			LockSignalMeta::LockWritesMetaDeque::iterator iiiter;
			for(iiiter=lsmeta->lk_wrs_metas.begin();
				iiiter!=lsmeta->lk_wrs_metas.end();iiiter++) {
				lk_wrs_meta=*iiiter;
				//same protecting lock
				if(lk_wrs_meta->lock_addr==lk_addr) {
					//traverse the write meta deque
					LockWritesMeta::WriteMetaDeque::iterator iiiiter;
					for(iiiiter=lk_wrs_meta->write_meta_deq.begin();
						iiiiter!=lk_wrs_meta->write_meta_deq.end();iiiiter++) {
						if((*iiiiter)->addr==addr)
							return lsmeta;
					}
				}
			}
			//important
			iiter++;
		}
	}
	return NULL;
}

void CondWaitDB::SetCondWaitCalledFunc(thread_t curr_thd_id,Inst *inst,
	bool flag)
{
	if(cwthd_cfmeta_map_.find(curr_thd_id)==cwthd_cfmeta_map_.end())
		cwthd_cfmeta_map_[curr_thd_id]=CondWaitCalledFuncMeta(inst,flag);
	else {
		cwthd_cfmeta_map_[curr_thd_id].inst=inst;
		cwthd_cfmeta_map_[curr_thd_id].flag=flag;
	}
}

void CondWaitDB::RemoveCondWaitCalledFunc(thread_t curr_thd_id)
{
	if(cwthd_cfmeta_map_.find(curr_thd_id)==cwthd_cfmeta_map_.end())
		cwthd_cfmeta_map_.erase(curr_thd_id);
}

bool CondWaitDB::CondWaitCalledFuncThread(thread_t curr_thd_id)
{
	return cwthd_cfmeta_map_.find(curr_thd_id)!=cwthd_cfmeta_map_.end() &&
		cwthd_cfmeta_map_[curr_thd_id].flag==true;
}

void CondWaitDB::AddCondWaitMeta(thread_t thd_id,LockSignalMeta *lsmeta,
	Inst *rdinst,address_t addr) 
{
	DEBUG_ASSERT(thd_cwmeta_map_.find(thd_id)==thd_cwmeta_map_.end());
	thd_cwmeta_map_[thd_id]=new CondWaitMeta(lsmeta,rdinst);
}

void CondWaitDB::RemoveCondWaitMeta(thread_t thd_id) 
{
	if(thd_cwmeta_map_.find(thd_id)==thd_cwmeta_map_.end())
		return ;
	CondWaitMeta *cwmeta=thd_cwmeta_map_[thd_id];
	//unuseful cwmeta
	cwmeta->lsmeta->expired=true;
	thd_cwmeta_map_.erase(thd_id);
	delete cwmeta;
}

bool CondWaitDB::CondWaitMetaThread(thread_t thd_id)
{
	return thd_cwmeta_map_.find(thd_id)!=thd_cwmeta_map_.end();
}

void CondWaitDB::SetCondWaitCallInst(thread_t thd_id)
{
	DEBUG_ASSERT(thd_cwmeta_map_.find(thd_id)!=thd_cwmeta_map_.end());
	thd_cwmeta_map_[thd_id]->callinst=cwthd_cfmeta_map_[thd_id].inst;
}

bool CondWaitDB::CondWaitRead(Inst *inst,RaceEventType type)
{
	if(type==RACE_EVENT_WRITE)
		return false;
	std::string file_name=inst->GetFileName();
	// size_t found=file_name.find_last_of("/");
	// file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	return exiting_cond_line_set_.find(FilenameAndLineHash(file_name,line))!=
		exiting_cond_line_set_.end();
}

bool CondWaitDB::CondWaitRead(std::string &file_name,int line,
	RaceEventType type)
{
	if(type==RACE_EVENT_WRITE)
		return false;
	std::stringstream ss;
	ss<<file_name<<line;
	return exiting_cond_line_set_.find(FilenameAndLineHash(file_name,line))!=
		exiting_cond_line_set_.end();
}

bool CondWaitDB::CondWaitCalledFunc(Inst *inst)
{
	std::string file_name=inst->GetFileName();
	// size_t found=file_name.find_last_of("/");
	// file_name=file_name.substr(found+1);
	int line=inst->GetLine();
	return exiting_cond_line_set_.find(FilenameAndLineHash(file_name,line))!=
		exiting_cond_line_set_.end();
}

void CondWaitDB::RemoveUnactivedLockWritesMeta(thread_t curr_thd_id,
	address_t lk_addr)
{
	// INFO_FMT_PRINT("===========remove unactived lock writes meta==========");
	//
	if(thd_lsmetas_map_.find(curr_thd_id)==thd_lsmetas_map_.end())
		return ;
	//get the current lock signal meta
	LockSignalMetaDeque *lsmetas=thd_lsmetas_map_[curr_thd_id];
	if(lsmetas->empty()) {
		thd_locks_map_[curr_thd_id].pop();
		return ;
	}
	LockSignalMeta *lsmeta=lsmetas->back();
	//if actived,we only remove the current lock
	if(lsmeta->actived) {
		lsmeta->signal_unlock=false;
		thd_locks_map_[curr_thd_id].pop();
		return ;		
	}
	//remove the current lock writes meta
	if(lsmeta->lk_wrs_metas.empty()) {
		thd_locks_map_[curr_thd_id].pop();
		return ;	
	}
	LockWritesMeta *lk_wrs_meta=lsmeta->lk_wrs_metas.back();
	// DEBUG_ASSERT(lk_wrs_meta->lock_addr==lk_addr);
	if(lk_wrs_meta->lock_addr!=lk_addr)
		return ;
	lsmeta->lk_wrs_metas.pop_back();
	delete lk_wrs_meta;
	//if no remainder lock writes meta
	if(lsmeta->lk_wrs_metas.empty()) {
		lsmetas->pop_back();
		delete lsmeta;
		//clear the thread's lock stack info
		// thd_locks_map_[curr_thd_id].pop_back();
	//update the thread's lastest lock info
	}else {
		// lk_wrs_meta=lsmeta->lk_wrs_metas.back();
		// thd_locks_map_[curr_thd_id]=lk_wrs_meta->lock_addr;
	}
	thd_locks_map_[curr_thd_id].pop();
}

void CondWaitDB::ActiveLockSignalMeta(thread_t curr_thd_id)
{
	// INFO_FMT_PRINT("===========active lock signal meta start\n");
	//get the current lock signal meta
	if(thd_lsmetas_map_.find(curr_thd_id)==thd_lsmetas_map_.end())
		return ;
	LockSignalMetaDeque *lsmetas=thd_lsmetas_map_[curr_thd_id];
	if(lsmetas->empty())
		return ;
	LockSignalMeta *lsmeta=lsmetas->back();
	lsmeta->actived=true;
	lsmeta->signal_unlock=true;
	// INFO_FMT_PRINT("===========active lock signal meta end\n");
}

address_t CondWaitDB::GetCondWaitMetaAddr(thread_t thd_id)
{
	DEBUG_ASSERT(thd_cwmeta_map_.find(thd_id)!=
		thd_cwmeta_map_.end());
	return thd_cwmeta_map_[thd_id]->addr;
}

bool CondWaitDB::LoadCondWait(const char *file_name)
{
	const char *delimit=" ",*fn=NULL;
	const char *psl=NULL,*pel=NULL;
	//cond_wait region
	const char *sl=NULL,*el=NULL,*ecl=NULL;
	char buffer[200];
	std::fstream in(file_name,std::ios::in);
	if(!in)
		return false;
	while(!in.eof()) {
		in.getline(buffer,200,'\n');
		fn=strtok(buffer,delimit);
		psl=strtok(NULL,delimit);
		pel=strtok(NULL,delimit);
		sl=strtok(NULL,delimit);
		el=strtok(NULL,delimit);

		// DEBUG_ASSERT(fn && psl && pel && sl && el);
		if(!fn)
			continue ;
		std::string fn_str(fn);
		if(cwloop_map_.find(fn_str)==cwloop_map_.end())
			cwloop_map_[fn_str]=new CondWaitLoopTable;
		int sl_int=atoi(sl),el_int=atoi(el);
		Loop *cwloop=new CondWaitLoop(sl_int,el_int);
		cwloop->SetProcedureScope(atoi(psl),atoi(pel));
		
		while((ecl=strtok(NULL,delimit))) {
			exiting_cond_line_set_.insert(
				FilenameAndLineHash(fn_str,atoi(ecl)));
			cwloop_map_[fn_str]->insert(std::make_pair(atoi(ecl),cwloop));
		}
	}
	in.close();
	return true;
}

bool CondWaitDB::ProcessCondWaitRead(thread_t curr_thd_id,Inst *curr_inst,
	VectorClock &curr_vc,address_t addr,std::string &file_name,int line)
{
	//current thread should be protected by at least one lock
	address_t lk_addr=0;
	if(thd_locks_map_[curr_thd_id].empty())
		return false;
	lk_addr=thd_locks_map_[curr_thd_id].top();
	//potential cond_wait read or cond_wait called function
	if(CondWaitRead(file_name,line,RACE_EVENT_READ) ||
		CondWaitCalledFuncThread(curr_thd_id)) {

		LockSignalMeta *lsmeta=CorrespondingLockSignalMeta(curr_vc,addr,
			lk_addr);
		//find the relevant lock signal meta, build the cond_wait meta
		if(lsmeta) {
// INFO_FMT_PRINT("============add cond wait meta============\n");
			AddCondWaitMeta(curr_thd_id,lsmeta,curr_inst,addr);
			SetCondWaitCallInst(curr_thd_id);
			return true;
		}
	}
	return false;
}

void CondWaitDB::ProcessSignalCondWaitSync(thread_t curr_thd_id,
	Inst *curr_inst,VectorClock &curr_vc)
{
	if(thd_cwmeta_map_.find(curr_thd_id)==thd_cwmeta_map_.end())
		return ;
	//
	CondWaitMeta *cwmeta=thd_cwmeta_map_[curr_thd_id];
	Inst *rdinst=cwmeta->rdinst;
	std::string file_name=rdinst->GetFileName();
	// size_t found=file_name.find_last_of("/");
	// file_name=file_name.substr(found+1);
	//lastest loop
	Loop *loop=NULL;
	//if the exiting condition is call func,may be the shared read occur in
	//the called func.
	if(cwloop_map_[file_name]->find(rdinst->GetLine()) == 
		cwloop_map_[file_name]->end()) {
		if(cwmeta->callinst)
			loop=(*cwloop_map_[file_name])[cwmeta->callinst->GetLine()];
	}else
		loop=(*cwloop_map_[file_name])[rdinst->GetLine()];
	DEBUG_ASSERT(loop);
	//first inst out of the loop
	if(!curr_inst || loop->OutLoopInProcedure(curr_inst->GetLine())) {
		//construct write->read sync
		curr_vc.Join(&cwmeta->lsmeta->vc);
		//the signal sync opetaion will increase the vc
		if(curr_inst) {
INFO_FMT_PRINT("+++++++++++++++write->read sync,curr inst:[%s]++++++++++++++\n",curr_inst->ToString().c_str());
		}
		//set the lock signal meta expired and it wil be free 
		//at the next iteration
		cwmeta->lsmeta->expired=true;
		thd_cwmeta_map_.erase(curr_thd_id);
		delete cwmeta;
	}
}

}//namespace race