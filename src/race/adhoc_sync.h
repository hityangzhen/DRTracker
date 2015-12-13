#ifndef __ADHOC_SYNC_H
#define __ADHOC_SYNC_H

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/vector_clock.h"
#include <deque>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

#define THRESHOLD 5

namespace race
{
class AdhocSync {
public:
	class Meta {
	public:
		Meta(Inst *i,address_t sa,address_t ea):
			inst(i),start_addr(sa),end_addr(ea) {}
		~Meta() {}
		Inst *inst;
		address_t start_addr;
		address_t end_addr;
	};

	class ReadMeta:public Meta {
	public:
		ReadMeta(Inst *i,address_t sa,address_t ea):Meta(i,sa,ea) {
			count=0;
			possible_spin=false;
			values=new char[ea-sa];
		}
		~ReadMeta() {
			delete []values;
		}
		void Reset(Inst *i,address_t sa, address_t ea) {
			inst=i;
			start_addr=sa;
			end_addr=ea;
			count=1;
			possible_spin=false;
			//free the older
			delete []values;
			values=new char[end_addr-start_addr];

			for(address_t iaddr=0;iaddr!=end_addr-start_addr;
				iaddr+=sizeof(char)) {
				values[iaddr]=*((char *)start_addr+iaddr);
			}
		}
		uint16 count;
		bool possible_spin;
		char *values;
	};

	class WriteMeta:public Meta {
	public:
		WriteMeta(Inst *i,address_t sa,address_t ea,thread_t thd_id,
			VectorClock vc):Meta(i,sa,ea),lastest_thd_id(thd_id),
			lastest_thd_vc(vc) {}
		~WriteMeta() {}
		thread_t GetLastestThread() { return lastest_thd_id; }
		thread_t lastest_thd_id;
		VectorClock lastest_thd_vc;
	};
	typedef std::deque<ReadMeta *> ReadMetas;
	typedef std::tr1::unordered_map<thread_t,ReadMetas *> ThreadReadMetasMap;
	typedef std::tr1::unordered_map<address_t,WriteMeta *> WriteMetaTable;
	typedef std::tr1::unordered_set<uint64> SyncInstPairSet;

	//keep the latest read metas
	ThreadReadMetasMap thd_rd_metas_map_;
	//keep the lastest write meta 
	WriteMetaTable wr_meta_table_;
	//keep the sync wr->rd pairs
	SyncInstPairSet sync_instpair_set_;
public:
	AdhocSync();
	~AdhocSync();

	void AddSyncInstPair(Inst *wr_inst,Inst *rd_inst) {
		uint64 x=reinterpret_cast<uint64>(wr_inst);
		uint64 y=reinterpret_cast<uint64>(rd_inst);
		//add the inst pair
		sync_instpair_set_.insert(x+(y>>32));
		//add the rd_inst singlely
		sync_instpair_set_.insert(y);
	}
	//
	bool SyncReadInst(Inst *rd_inst) {
		uint64 y=reinterpret_cast<uint64>(rd_inst);
		return sync_instpair_set_.find(y)!=sync_instpair_set_.end();
	}

	//whether in current thread's read meta 
	ReadMeta *FindReadMeta(thread_t curr_thd_id,Inst *rd_inst);
	void AddReadMeta(thread_t curr_thd_id,Inst *rd_inst,
		address_t start_addr,address_t end_addr);
	//whether current thread read sync with previouse write
	WriteMeta* WriteReadSync(thread_t curr_thd_id,Inst *rd_inst,
		address_t start_addr,address_t end_addr);

	void AddOrUpdateWriteMeta(thread_t curr_thd_id,VectorClock *vc,
		Inst *rw_inst,address_t start_addr,address_t end_addr);

	void BuildWriteReadSync(WriteMeta *wr_meta,VectorClock *wr_vc,
	VectorClock *curr_vc);

	void SameAddrReadMetas(thread_t curr_thd_id,address_t start_addr,
		address_t end_addr,std::set<ReadMeta *> &result);

	static uint16 rd_metas_len;
private:

	bool BytesEqual(ReadMeta *rd_meta,address_t start_addr,
		address_t end_addr) {
		for(address_t iaddr=0;iaddr!=end_addr-start_addr;
			iaddr+=sizeof(char)) {
			if(rd_meta->values[iaddr]!=*((char*)start_addr+iaddr))
				return false;
		}
		return true;
	};
	DISALLOW_COPY_CONSTRUCTORS(AdhocSync);
};

} //namespace 

#endif
