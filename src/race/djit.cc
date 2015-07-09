#include "race/djit.h"
#include "core/log.h"

namespace race {

Djit::Djit():track_racy_inst_(false) {}

Djit::~Djit() {}

void Djit::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_djit",
		"whether enable the djit data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool Djit::Enabled()
{
	return knob_->ValueBool("enable_djit");
}

void Djit::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

Detector::Meta *Djit::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new DjitMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void Djit::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process read\n");
	//cast the meta
	DjitMeta *djit_meta=dynamic_cast<DjitMeta *>(meta);
	DEBUG_ASSERT(djit_meta);
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];

	// INFO_FMT_PRINT("addr:%lx\n",djit_meta->addr);
	// INFO_FMT_PRINT("writer_vc:%s\n",djit_meta->reader_vc.ToString().c_str());
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc->ToString().c_str());

	//same time-frame's read access
	if(djit_meta->reader_vc.GetClock(curr_thd_id) == 
		curr_vc_map_[curr_thd_id]->GetClock(curr_thd_id))
		return ;

	//check writers
	VectorClock &writer_vc=djit_meta->writer_vc;
	if(!writer_vc.HappensBefore(curr_vc)) {
		
		//PrintDebugRaceInfo("DJIT",WRITETOREAD,djit_meta,curr_thd_id,inst);

		//mark the meta as racy
		djit_meta->racy=true;
		//RAW race detected ,report them
		for(writer_vc.IterBegin();!writer_vc.IterEnd();writer_vc.IterNext()) {
			thread_t thd_id=writer_vc.IterCurrThd();
			timestamp_t clk=writer_vc.IterCurrClk();
			//ensure which thread's write access is newer than current thread
			if(curr_thd_id!=thd_id &&clk > curr_vc->GetClock(thd_id)) {
				DEBUG_ASSERT(djit_meta->writer_inst_table.find(thd_id)!=
					djit_meta->writer_inst_table.end());
				Inst *writer_inst=djit_meta->writer_inst_table[thd_id];
				ReportRace(djit_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,inst,RACE_EVENT_READ);
			}
		}
	}
	
	//update meta data-only the current thread's entry
	djit_meta->reader_vc.SetClock(curr_thd_id,curr_vc->GetClock(curr_thd_id));
	djit_meta->reader_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		djit_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process read end\n");
}

void Djit::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	//INFO_PRINT("process write\n");
	DjitMeta *djit_meta=dynamic_cast<DjitMeta*>(meta);
	DEBUG_ASSERT(djit_meta);
	//get the current vector clock
	VectorClock *curr_vc=curr_vc_map_[curr_thd_id];
	// INFO_FMT_PRINT("addr:%lx\n",djit_meta->addr);
	// INFO_FMT_PRINT("writer_vc:%s\n",djit_meta->writer_vc.ToString().c_str());
	// INFO_FMT_PRINT("curr_vc:%s\n",curr_vc->ToString().c_str());
	//same time-frame's write access
	if(djit_meta->writer_vc.GetClock(curr_thd_id) == 
		curr_vc->GetClock(curr_thd_id) )
		return ;

	VectorClock &writer_vc=djit_meta->writer_vc;
	VectorClock &reader_vc=djit_meta->reader_vc;

	
	//check writes
	if(!writer_vc.HappensBefore(curr_vc)) {

		//PrintDebugRaceInfo("DJIT",WRITETOWRITE,djit_meta,curr_thd_id,inst);

    	//mark the meta as racy
    	djit_meta->racy=true;
    	//WAW race detected , report them
    	for(writer_vc.IterBegin();!writer_vc.IterEnd();writer_vc.IterNext()) {
    		thread_t thd_id=writer_vc.IterCurrThd();
    		timestamp_t clk=writer_vc.IterCurrClk();

    		if(curr_thd_id!=thd_id && clk > curr_vc->GetClock(thd_id)) {
    			DEBUG_ASSERT(djit_meta->writer_inst_table.find(thd_id)!=
    				djit_meta->writer_inst_table.end());
    			Inst *writer_inst=djit_meta->writer_inst_table[thd_id];
    			//report the race
    			ReportRace(djit_meta,thd_id,writer_inst,RACE_EVENT_WRITE,
    				curr_thd_id,inst,RACE_EVENT_WRITE);
    		}
    	}
	}

	//check read
	if(!reader_vc.HappensBefore(curr_vc)) {
		
		//PrintDebugRaceInfo("DJIT",READTOWRITE,djit_meta,curr_thd_id,inst);

    	//mark the meta as racy
    	djit_meta->racy=true;
    	//WAR race detected , report them
    	for(reader_vc.IterBegin();!reader_vc.IterEnd();reader_vc.IterNext()) {
    		thread_t thd_id=reader_vc.IterCurrThd();
    		timestamp_t clk=reader_vc.IterCurrClk();

    		if(curr_thd_id!=thd_id && clk > curr_vc->GetClock(thd_id)) {
    			DEBUG_ASSERT(djit_meta->reader_inst_table.find(thd_id)!=
    				djit_meta->reader_inst_table.end());
    			Inst *reader_inst=djit_meta->reader_inst_table[thd_id];
    			//report the race
    			ReportRace(djit_meta,thd_id,reader_inst,RACE_EVENT_READ,
    				curr_thd_id,inst,RACE_EVENT_WRITE);
    		}
    	}
	}
	//update meta info
	writer_vc.SetClock(curr_thd_id,curr_vc->GetClock(curr_thd_id));
	djit_meta->writer_inst_table[curr_thd_id]=inst;
	//update race inst set if needed
	if(track_racy_inst_)
		djit_meta->race_inst_set.insert(inst);
	//INFO_PRINT("process write end\n");
}

void Djit::ProcessFree(Meta *meta)
{
	
	DjitMeta *djit_meta=dynamic_cast<DjitMeta *>(meta);
	//ensure  the DjitMeta ,exclude others like MutexMeta(only include 
	//vector clock)
	DEBUG_ASSERT(djit_meta);
	//update the racy inst set if needed
	if(track_racy_inst_ && djit_meta->racy) {
		for(DjitMeta::InstSet::iterator it=djit_meta->race_inst_set.begin();
			it!=djit_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}
	vc_mem_size_+=djit_meta->writer_vc.GetMemSize();
	vc_mem_size_+=djit_meta->reader_vc.GetMemSize();

	//only destructor the DjitMeta
	delete djit_meta;
}

} //namespace race