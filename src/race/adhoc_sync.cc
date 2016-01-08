#include "race/adhoc_sync.h"
#include "core/log.h"

namespace race
{

uint16 AdhocSync::rd_metas_len=3;

AdhocSync::AdhocSync()
{}

AdhocSync::~AdhocSync()
{
	for(ThreadReadMetasMap::iterator iter=thd_rd_metas_map_.begin();
		iter!=thd_rd_metas_map_.end();iter++) {
		for(ReadMetas::iterator iiter=iter->second->begin();
			iiter!=iter->second->end();iiter++)
			delete *iiter;
		delete iter->second;
	}

	for(WriteMetaTable::iterator iter=wr_meta_table_.begin();
		iter!=wr_meta_table_.end();iter++)
		delete iter->second;

	//clear loops
	for(LoopMap::iterator iter=loop_map_.begin();iter!=loop_map_.end();
		iter++)
		delete iter->second;
}

AdhocSync::ReadMeta* AdhocSync::FindReadMeta(thread_t curr_thd_id,Inst *rd_inst)
{
	if(thd_rd_metas_map_.find(curr_thd_id)==thd_rd_metas_map_.end())
		return NULL;
	ReadMetas *rd_metas=thd_rd_metas_map_[curr_thd_id];
	for(ReadMetas::reverse_iterator iter=rd_metas->rbegin();
		iter!=rd_metas->rend();iter++)
		if((*iter)->inst==rd_inst)
			return *iter;
	return NULL;
}

void AdhocSync::SameAddrReadMetas(thread_t curr_thd_id,address_t start_addr,
	address_t end_addr,std::set<AdhocSync::ReadMeta *> &result)
{
	if(thd_rd_metas_map_.find(curr_thd_id)==thd_rd_metas_map_.end())
		return ;
	ReadMetas *rd_metas=thd_rd_metas_map_[curr_thd_id];
	for(ReadMetas::iterator iter=rd_metas->begin();
		iter!=rd_metas->end();iter++)
		if((*iter)->start_addr==start_addr && 
			(*iter)->end_addr==end_addr)
			result.insert(*iter);
}

void AdhocSync::AddReadMeta(thread_t curr_thd_id,Inst *rd_inst,
	address_t start_addr,address_t end_addr)
{
	if(thd_rd_metas_map_.find(curr_thd_id)==thd_rd_metas_map_.end())
		thd_rd_metas_map_[curr_thd_id]=new ReadMetas;
	ReadMetas *rd_metas=thd_rd_metas_map_[curr_thd_id];
	//too many read metas
	if(rd_metas->size()==rd_metas_len) {
		ReadMeta *rd_meta=rd_metas->front();
		rd_metas->pop_front();
		delete rd_meta;
	}
	//set the initialize value
	ReadMeta *rd_meta=new ReadMeta(rd_inst,start_addr,end_addr);
	for(address_t iaddr=0;iaddr!=end_addr-start_addr;
		iaddr+=sizeof(char)) {
		rd_meta->values[iaddr]=*((char *)start_addr+iaddr);
	}
	rd_metas->push_back(rd_meta);
}

void AdhocSync::AddOrUpdateWriteMeta(thread_t curr_thd_id,VectorClock *vc,
	Inst *wr_inst,address_t start_addr,address_t end_addr)
{
	if(wr_meta_table_.find(start_addr)==wr_meta_table_.end())
		wr_meta_table_[start_addr]=new WriteMeta(wr_inst,start_addr,
			end_addr,curr_thd_id,*vc);
	else {
		DEBUG_ASSERT(wr_meta_table_[start_addr]);
		WriteMeta *wr_meta=wr_meta_table_[start_addr];
		wr_meta->start_addr=start_addr;
		wr_meta->end_addr=end_addr;
		wr_meta->inst=wr_inst;
		wr_meta->lastest_thd_id=curr_thd_id;
		wr_meta->lastest_thd_vc=*vc;
	}
}

bool AdhocSync::LoadLoops(const char *file_name)
{
	const char *delimit=" ",*fn=NULL,*sl=NULL,*el=NULL;
	char buffer[200];
	std::fstream in(file_name,std::ios::in);
	if(!in)
		return false;
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
		loop_map_[fn_str]->insert(std::make_pair(sl_int,Loop(sl_int,el_int)));
	}
	in.close();
	return true;
}

/**
 * do the write->read sync relation analysis
 */
AdhocSync::WriteMeta* AdhocSync::WriteReadSync(thread_t curr_thd_id,Inst *rd_inst,
	address_t start_addr,address_t end_addr)
{
	if(SyncReadInst(rd_inst))
		return NULL;
	ReadMeta *rd_meta=NULL;
	if((rd_meta=FindReadMeta(curr_thd_id,rd_inst))!=NULL) {
INFO_FMT_PRINT("=================read metas size:[%ld]==============\n",
		thd_rd_metas_map_[curr_thd_id]->size());
		//if the target address changed
		if(rd_meta->start_addr!=start_addr || rd_meta->end_addr!=end_addr)
			rd_meta->Reset(rd_inst,start_addr,end_addr);
		else {
			//use the inefficient comparison
			if(BytesEqual(rd_meta,start_addr,end_addr)) {
				//haven't been in spin
				if(!rd_meta->possible_spin) {
					rd_meta->count++;
					if(rd_meta->count==THRESHOLD) {
						rd_meta->possible_spin=true;
						rd_meta->count=THRESHOLD;
					}
				}
			//may be corresponding write access
			} else {
				//lastest write is current thread or non-spin
				if((wr_meta_table_.find(start_addr)!=wr_meta_table_.end() &&
					wr_meta_table_[start_addr]->lastest_thd_id==curr_thd_id)
					|| !rd_meta->possible_spin) {
					rd_meta->Reset(rd_inst,start_addr,end_addr);				
				} else {
					//keep the write-read sync relation
					AddSyncInstPair(wr_meta_table_[start_addr]->inst,rd_inst);
					return wr_meta_table_[start_addr];	
				}
			}
		}
	//first shown
	}else
		AddReadMeta(curr_thd_id,rd_inst,start_addr,end_addr);
	return NULL;
}

void AdhocSync::BuildWriteReadSync(WriteMeta *wr_meta,VectorClock *wr_vc,
	VectorClock *curr_vc)
{
	wr_vc->Increment(wr_meta->lastest_thd_id);
	curr_vc->Join(&wr_meta->lastest_thd_vc);
}

} //namespace race
