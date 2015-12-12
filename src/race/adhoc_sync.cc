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
INFO_FMT_PRINT("=================start_addr:[%lx],end_start:[%lx]===================\n",
	start_addr,end_addr);
	for(address_t iaddr=0;iaddr!=end_addr-start_addr;
		iaddr+=sizeof(char)) {
		rd_meta->values[iaddr]=*((char *)start_addr+iaddr);
	}
	rd_metas->push_back(rd_meta);
}

void AdhocSync::AddOrUpdateWriteMeta(thread_t curr_thd_id,Inst *wr_inst,
	address_t start_addr,address_t end_addr)
{
	if(wr_meta_table_.find(start_addr)==wr_meta_table_.end())
		wr_meta_table_[start_addr]=new WriteMeta(wr_inst,start_addr,
			end_addr,curr_thd_id);
	else {
		DEBUG_ASSERT(wr_meta_table_[start_addr]);
		WriteMeta *wr_meta=wr_meta_table_[start_addr];
		wr_meta->start_addr=start_addr;
		wr_meta->end_addr=end_addr;
		wr_meta->inst=wr_inst;
		wr_meta->lastest_thd_id=curr_thd_id;
	}
}

/**
 * do the write->read sync relation analysis
 */
AdhocSync::WriteMeta* AdhocSync::WriteReadSync(thread_t curr_thd_id,Inst *rd_inst,
	address_t start_addr,address_t end_addr)
{
INFO_FMT_PRINT("=================write read sync:[%lx],value:[%d]===================\n",
	curr_thd_id,*(int *)start_addr);
	if(SyncReadInst(rd_inst))
		return NULL;
	ReadMeta *rd_meta=NULL;
	if((rd_meta=FindReadMeta(curr_thd_id,rd_inst))!=NULL) {
		//if the target address changed
		if(rd_meta->start_addr!=start_addr || rd_meta->end_addr!=end_addr)
			rd_meta->Reset(rd_inst,start_addr,end_addr);
		else {
INFO_PRINT("=====================address equal====================\n");
			//use the inefficient comparison
			if(BytesEqual(rd_meta,start_addr,end_addr)) {
INFO_PRINT("=====================bytes equal====================\n");
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
	}else {
		AddReadMeta(curr_thd_id,rd_inst,start_addr,end_addr);
INFO_PRINT("===================add read meta=====================\n");
	}
	return NULL;
}

} //namespace race
