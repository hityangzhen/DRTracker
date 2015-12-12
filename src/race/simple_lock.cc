#include "race/simple_lock.h"
#include "core/log.h"

namespace race {

SimpleLock::SimpleLock():track_racy_inst_(false)
{}

SimpleLock::~SimpleLock()
{}


uint32 SimpleLock::SlMeta::max_list_len=1;

void SimpleLock::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_simple_lock",
		"whether enable the simple_lock data race detector","0");
	knob_->RegisterInt("simple_lock_max_list_len",
		"set the simple_lock max_list_len","1");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool SimpleLock::Enabled()
{
	return knob_->ValueBool("enable_simple_lock");
}

void SimpleLock::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
	SlMeta::SetMaxListLen(knob_->ValueInt("simple_lock_max_list_len"));
}

void SimpleLock::AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);	
	//only increment thread's lock count
	if(curr_lc_table_.find(curr_thd_id)==curr_lc_table_.end())
		curr_lc_table_[curr_thd_id]=1;
	else
		++curr_lc_table_[curr_thd_id];
}

void SimpleLock::BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	//only decrement thread's lock count
	ThreadLockCountTable::iterator it=curr_lc_table_.find(curr_thd_id);
	DEBUG_ASSERT(it!=curr_lc_table_.end());
	--curr_lc_table_[curr_thd_id];
	DEBUG_ASSERT(curr_lc_table_[curr_thd_id]>=0);
}

void SimpleLock::AfterPthreadRwlockRdlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}
	
void SimpleLock::AfterPthreadRwlockWrlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	AfterPthreadMutexLock(curr_thd_id,curr_thd_clk,inst,addr);
}

void SimpleLock::BeforePthreadRwlockUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
	Inst *inst,address_t addr)
{
	BeforePthreadMutexUnlock(curr_thd_id,curr_thd_clk,inst,addr);
}

Detector::Meta *SimpleLock::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new SlMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}


void SimpleLock::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_FMT_PRINT("process read:%lx\n",meta->addr);
	SlMeta *sl_meta=dynamic_cast<SlMeta*>(meta);
	DEBUG_ASSERT(sl_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);
	//
	if(!sl_meta->reader_clpl_map[curr_thd_id])
		sl_meta->reader_clpl_map[curr_thd_id]=new SlMeta::ClockLockcntPairList;
	SlMeta::ClockLockcntPairList *reader_clpl=sl_meta->reader_clpl_map[curr_thd_id];
	SlMeta::InstList *reader_inst_list=NULL;

	int update=0;

	//first read
	if(reader_clpl->size()==0) {
		if(!sl_meta->reader_instlist_table[curr_thd_id])
			sl_meta->reader_instlist_table[curr_thd_id]=new SlMeta::InstList;
	}
	reader_inst_list=sl_meta->reader_instlist_table[curr_thd_id];

	//different epoch lock count
	if(reader_clpl->size()==0 || reader_clpl->back().first!=curr_clk) {
		reader_clpl->push_back(SlMeta::ClockLockcntPair(curr_clk,curr_lc_table_[curr_thd_id]));
		if(reader_clpl->size()>SlMeta::max_list_len)
			reader_clpl->erase(reader_clpl->begin());
		update=1;
	}
	//update the smaller lock count
	else if(reader_clpl->back().first==curr_clk && 
								reader_clpl->back().second>curr_lc_table_[curr_thd_id] ) {
		reader_clpl->back().second=curr_lc_table_[curr_thd_id];
		update=true;
		update=-1;
	}
	//write-read race
	SlMeta::ThreadClockLockcntPairListMap::iterator it=sl_meta->writer_clpl_map.begin();
	
	for(;it!=sl_meta->writer_clpl_map.end();it++) {
		SlMeta::InstList *writer_inst_list=sl_meta->writer_instlist_table[it->first];
		if(it->first==curr_thd_id || writer_inst_list==NULL)
			continue ;
		SlMeta::ClockLockcntPairList *writer_clpl=it->second;
		SlMeta::ClockLockcntPairList::iterator clp_it=writer_clpl->begin();
		SlMeta::InstList::iterator inst_it=writer_inst_list->begin();

		//INFO_FMT_PRINT("writer_clpl size:%ld\n",writer_clpl->size());
		

		for(;clp_it!=writer_clpl->end() && inst_it!=writer_inst_list->end();
			clp_it++,inst_it++) {
			SlMeta::ClockLockcntPair &writer_clp=*clp_it;
			//non-ordered writer
			timestamp_t thd_clk=curr_vc->GetClock(it->first);
			// INFO_FMT_PRINT("writer_clp thd:%lx,writer_clp clk:%ld\n",
			// 	it->first,writer_clp.first);
			// INFO_FMT_PRINT("curr thd:%lx,curr clk:%ld\n",
			// 	curr_thd_id,curr_clk);
			// INFO_FMT_PRINT("writer_clp lockcount:%d,curr lockcount:%d\n",
			// 	writer_clp.second,curr_lc_table_[curr_thd_id]);

			if(writer_clp.first>thd_clk &&
				(writer_clp.second==0 || curr_lc_table_[curr_thd_id]==0)) {

				//PrintDebugRaceInfo("SIMPLELOCK",WRITETOREAD,sl_meta,curr_thd_id,inst);

				sl_meta->racy=true;
				ReportRace(sl_meta,it->first,*inst_it,RACE_EVENT_WRITE,
													curr_thd_id,inst,RACE_EVENT_READ);
			}
		}
	}
	//update or insert inst
	if(update==1) {
		reader_inst_list->push_back(inst);
		if(reader_inst_list->size()>SlMeta::max_list_len)
			reader_inst_list->erase(reader_inst_list->begin());
	}else if(update==-1){
		reader_inst_list->back()=inst;
	}

	DEBUG_ASSERT(reader_inst_list->size()==reader_clpl->size());

	if(track_racy_inst_)
		sl_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process read end\n");
}


void SimpleLock::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_FMT_PRINT("process write:%lx\n",meta->addr);
	SlMeta *sl_meta=dynamic_cast<SlMeta*>(meta);
	DEBUG_ASSERT(sl_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	timestamp_t curr_clk=curr_vc->GetClock(curr_thd_id);
	//
	if(!sl_meta->writer_clpl_map[curr_thd_id])
		sl_meta->writer_clpl_map[curr_thd_id]=new SlMeta::ClockLockcntPairList;
	SlMeta::ClockLockcntPairList *writer_clpl=sl_meta->writer_clpl_map[curr_thd_id];
	SlMeta::InstList *writer_inst_list=NULL;

	//0  :nothing to do
	//1  :new epoch
	//-1 :update
	int update=0;
	//first write
	if(writer_clpl->size()==0) {
		if(!sl_meta->writer_instlist_table[curr_thd_id])
			sl_meta->writer_instlist_table[curr_thd_id]=new SlMeta::InstList;
	}
	writer_inst_list=sl_meta->writer_instlist_table[curr_thd_id];

	//different epoch lock count
	if(writer_clpl->size()==0 || writer_clpl->back().first!=curr_clk) {
		writer_clpl->push_back(SlMeta::ClockLockcntPair(curr_clk,curr_lc_table_[curr_thd_id]));	
		if(writer_clpl->size()>SlMeta::max_list_len) 
			writer_clpl->erase(writer_clpl->begin());
		update=1;
	}
	//update the smaller lock count
	else if(writer_clpl->back().first==curr_clk && 
								writer_clpl->back().second>curr_lc_table_[curr_thd_id] ) {
		writer_clpl->back().second=curr_lc_table_[curr_thd_id];
		update=-1;
	}
	//INFO_FMT_PRINT("writer_clpl size:%ld\n",writer_clpl->size());
	//write-write race
	SlMeta::ThreadClockLockcntPairListMap::iterator it=sl_meta->writer_clpl_map.begin();

	for(;it!=sl_meta->writer_clpl_map.end();it++) {
		SlMeta::InstList *writer_inst_list2=sl_meta->writer_instlist_table[it->first];
		if(it->first==curr_thd_id || writer_inst_list2==NULL)
			continue;

		SlMeta::ClockLockcntPairList *writer_clpl=it->second;
		SlMeta::ClockLockcntPairList::iterator clp_it=writer_clpl->begin();	
		
		SlMeta::InstList::iterator inst_it=writer_inst_list2->begin();

		for(;clp_it!=writer_clpl->end() && inst_it!=writer_inst_list2->end();
			clp_it++,inst_it++) {
			SlMeta::ClockLockcntPair &writer_clp=*clp_it;
			//non-ordered write
			timestamp_t thd_clk=curr_vc->GetClock(it->first);

			// INFO_FMT_PRINT("writer_clp thd:%lx,writer_clp clk:%ld\n",
			// 	it->first,writer_clp.first);
			// INFO_FMT_PRINT("curr thd:%lx,curr clk:%ld\n",
			// 	curr_thd_id,curr_clk);
			// INFO_FMT_PRINT("thd_clk:%ld\n",thd_clk);

			if(writer_clp.first>thd_clk &&
				(writer_clp.second==0 || curr_lc_table_[curr_thd_id]==0)) {

				//PrintDebugRaceInfo("SIMPLELOCK",WRITETOWRITE,sl_meta,curr_thd_id,inst);

				sl_meta->racy=true;
				ReportRace(sl_meta,it->first,*inst_it,RACE_EVENT_WRITE,
													curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
	}

	//read-write race
	it=sl_meta->reader_clpl_map.begin();
	
	for(;it!=sl_meta->reader_clpl_map.end() ;it++) {
		SlMeta::InstList *reader_inst_list2=sl_meta->reader_instlist_table[it->first];
		if(it->first==curr_thd_id || reader_inst_list2==NULL)
			continue;

		SlMeta::ClockLockcntPairList *reader_clpl=it->second;
		SlMeta::ClockLockcntPairList::iterator clp_it=reader_clpl->begin();
			
		SlMeta::InstList::iterator inst_it=reader_inst_list2->begin();

		for(;clp_it!=reader_clpl->end() && inst_it!=reader_inst_list2->end();
			clp_it++,inst_it++) {
			SlMeta::ClockLockcntPair &reader_clp=*clp_it;

			timestamp_t thd_clk=curr_vc->GetClock(it->first);

			//non-ordered read
			// INFO_FMT_PRINT("reader_clp thd:%lx,reader_clp clk:%ld\n",
			// 	it->first,reader_clp.first);
			// INFO_FMT_PRINT("curr thd:%lx,curr clk:%ld\n",
			// 	curr_thd_id,curr_clk);
			// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc->ToString().c_str());
			// INFO_FMT_PRINT("reader_clp lockcount:%d,curr thread lockcount:%d\n",
			// 	reader_clp.second,curr_lc_table_[curr_thd_id]);
			if(reader_clp.first>thd_clk &&
				(reader_clp.second==0 || curr_lc_table_[curr_thd_id]==0)) {

				// PrintDebugRaceInfo("SIMPLELOCK",READTOWRITE,sl_meta,curr_thd_id,inst);

				sl_meta->racy=true;
				ReportRace(sl_meta,it->first,*inst_it,RACE_EVENT_READ,
													curr_thd_id,inst,RACE_EVENT_WRITE);
			}
		}
	}
	if(update==1) {
		writer_inst_list->push_back(inst);
		if(writer_inst_list->size()>SlMeta::max_list_len)
			writer_inst_list->erase(writer_inst_list->begin());
	}else if(update==-1){
		writer_inst_list->back()=inst;
	}
	DEBUG_ASSERT(writer_inst_list->size()==writer_clpl->size());

	if(track_racy_inst_)
		sl_meta->race_inst_set.insert(inst);

	//INFO_PRINT("process write end\n");
}

void SimpleLock::ProcessFree(Meta *meta)
{
	SlMeta *sl_meta=dynamic_cast<SlMeta *>(meta);
	DEBUG_ASSERT(sl_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && sl_meta->racy) {
		for(SlMeta::InstSet::iterator it=sl_meta->race_inst_set.begin();
			it!=sl_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}

	SlMeta::InstListMap::iterator inst_list_it=sl_meta->writer_instlist_table.begin();
	for(;inst_list_it!=sl_meta->writer_instlist_table.end();inst_list_it++)
		delete inst_list_it->second;

	inst_list_it=sl_meta->reader_instlist_table.begin();
	for(;inst_list_it!=sl_meta->reader_instlist_table.end();inst_list_it++)
		delete inst_list_it->second;	

	SlMeta::ThreadClockLockcntPairListMap::iterator clpl_iter=
													sl_meta->reader_clpl_map.begin();
	for(;clpl_iter!=sl_meta->reader_clpl_map.end();clpl_iter++)
		delete clpl_iter->second;

	clpl_iter=sl_meta->writer_clpl_map.begin();
	for(;clpl_iter!=sl_meta->writer_clpl_map.end();clpl_iter++)
		delete clpl_iter->second;

	delete sl_meta;
}


}//namespace race