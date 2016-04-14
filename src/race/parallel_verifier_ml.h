#ifndef __RACE_PARALLEL_VERIFIER_ML_H
#define __RACE_PARALLEL_VERIFIER_ML_H

#include "race/verifier_ml.h"
#include "core/pin_util.h"
#include <queue>
#include "core/log.h"
namespace race
{
class ParallelVerifierMl:public VerifierMl {
public:
	ParallelVerifierMl();
	virtual ~ParallelVerifierMl();

	bool Enabled();
	void Register();

	virtual void ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id);
	virtual void ThreadExit(thread_t curr_thd_id,timestamp_t curr_thd_clk);
	//thread create-join
	virtual void AfterPthreadCreate(thread_t curr_thd_id,timestamp_t curr_thd_clk,
  		Inst *inst,thread_t child_thd_id);
	virtual void AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,thread_t child_thd_id);
	//read-write
	virtual void BeforeMemRead(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t addr,size_t size);
	virtual void BeforeMemWrite(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t addr,size_t size);
	//barrier
	virtual void BeforePthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
    	Inst *inst,address_t addr);
	virtual void AfterPthreadBarrierWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t addr);
	//cond_wait
	virtual void BeforePthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t cond_addr,address_t mutex_addr);
	virtual void AfterPthreadCondWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
		Inst *inst,address_t cond_addr,address_t mutex_addr);
	//semaphore
	virtual void BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t addr);
	virtual void AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
    	Inst *inst,address_t addr);
public:
	typedef enum {
		AVAILABLE,
		BEING_POSTPONED,
		AFTER_POSTPONED,
		TERMINAL
	} THREAD_STATUS;
	//synchronization object is shared between VerifyRequest and HtyDtcRequest
	class SyncObject {
	public:
		SyncObject():ref(0) {}
		~SyncObject() {}
		VectorClock vc;
		LockSet rd_ls;
		LockSet wr_ls;
		int ref;
	};
	class VerifyRequest {
	public:
		VerifyRequest(thread_t t,Inst *i,address_t a,size_t s,RaceEventType rt)
		:thd_id(t),inst(i),addr(a),size(s),type(rt),sync_obj(NULL) {}
		~VerifyRequest() {}
		thread_t thd_id;
		Inst *inst;
		address_t addr;
		size_t size;
		RaceEventType type;
		SyncObject *sync_obj;
	};
	class ThreadLocalStore {
	public:
		ThreadLocalStore():status(AVAILABLE) {}
		~ThreadLocalStore() {}

		MetaSet hy_metas;
		MetaSet pp_metas;
		VectorClock vc;
		LockSet rd_ls;
		LockSet wr_ls;
		PStmtMetasMap pstmt_metas_map;
		volatile THREAD_STATUS status;
	};

	class HtyDtcRequest {
	public:
		HtyDtcRequest(Meta *m,PStmt *p,Inst *i,thread_t t,RaceEventType rt,
		SyncObject *obj):meta(m),pstmt(p),inst(i),thd_id(t),type(rt),sync_obj(obj) {}
		~HtyDtcRequest() {}
		Meta *meta;
		PStmt *pstmt;
		Inst *inst;
		thread_t thd_id;
		RaceEventType type;
		SyncObject *sync_obj;
	};
	typedef std::queue<VerifyRequest *> VerifyRequestQueue;
	typedef std::queue<HtyDtcRequest *> HtyDtcRequestQueue;
	typedef std::map<thread_t,HtyDtcRequestQueue *> HtyDtcReqQueueTable;

	void SetTlsKey(TLS_KEY tls) { tls_key_=tls; }
	void SetParallelVerifierNumber(int num) { prl_vrf_num_=num; }
	void PushVerifyRequest(VerifyRequest *req,ThreadLocalStore *tls);
	VerifyRequest* GetVerifyRequest() {
		if(vrf_req_que_.empty())
			return NULL;
		VerifyRequest *req=vrf_req_que_.front();
		return req;
	}
	void PopVerifyRequest() {
		if(!vrf_req_que_.empty())
			vrf_req_que_.pop();
	}
	bool VerifyRequestQueueEmpty() { return vrf_req_que_.empty(); }
	void ClearVerifyRequest(VerifyRequest *req) {
		delete req;
	}
	void CreateHtyDtcRequestQueue(thread_t thd_id) {
		htydtc_reqque_table_[thd_id]=new HtyDtcRequestQueue;
	}
	bool HtyDtcRequestQueueEmpty(thread_t thd_id) {
		return htydtc_reqque_table_[thd_id]->empty();
	}
	HtyDtcRequest *PopHtyDtcRequest(thread_t thd_id) {
		HtyDtcRequestQueue *req_deq=htydtc_reqque_table_[thd_id];
		if(req_deq->empty())
			return NULL;
		HtyDtcRequest *req=req_deq->front();
		req_deq->pop();
		return req;
	}
	void ClearHtyDtcRequest(HtyDtcRequest *req) {
		if(--req->sync_obj->ref==0)
			delete req->sync_obj;
		delete req;
	}
	//wrapper
	void HistoryDetection(HtyDtcRequest *req) {
		HistoryDetection(req->meta,req->pstmt,req->inst,req->thd_id,req->type,
			req->sync_obj);
	}
	//wrapper
	void ProcessReadOrWrite(VerifyRequest *req);
protected:
	void ProcessVerifyRequest(VerifyRequest *req);
	void PostponeThread(thread_t curr_thd_id);
	void ClearPostponedThreadMetas(thread_t curr_thd_id);
	void ProcessPostMutexLock(thread_t curr_thd_id,MutexMeta *mutex_meta);
	void ProcessPreMutexUnlock(thread_t curr_thd_id,MutexMeta *mutex_meta);
	void ProcessPostRwlockRdlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPostRwlockWrlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessPreRwlockUnlock(thread_t curr_thd_id,RwlockMeta *rwlock_meta);
	void ProcessSignal(thread_t curr_thd_id,CondMeta *cond_meta);
	PStmt *GetFirstPStmtSet(thread_t curr_thd_id,Inst *inst,PStmtSet &first_pstmts);
	void ProcessReadOrWrite(thread_t curr_thd_id,Inst *inst,address_t addr,
		size_t size,RaceEventType type);
	void WakeUpPostponeThread(thread_t thd_id);
	void HandleRace(std::map<thread_t,bool> &pp_thd_map,thread_t curr_thd_id);
	void HandleNoRace(thread_t curr_thd_id);
	void AddMetaSnapshot(Meta *meta,thread_t curr_thd_id,timestamp_t curr_thd_clk,
		RaceEventType type,Inst *inst,PStmt *s);
	void ClearFullyVerifierPStmtMetas(PStmt *pstmt);
	void ClearWhenVerifyRequestInvalid(ThreadLocalStore *tls);
	VERIFY_RESULT WaitVerification(address_t start_addr,address_t end_addr,PStmt *first_pstmt,
		PStmt *second_pstmt,Inst *inst,thread_t curr_thd_id,RaceEventType type,
		std::map<thread_t,bool> &pp_thd_map);
	RaceType HistoryRace(MetaSnapshot *meta_ss,timestamp_t curr_thd_clk,LockSet &rd_ls,
		LockSet &wr_ls,RaceEventType curr_type);
	void DistributeHtyDtcRequest(Meta *meta,Inst *inst,thread_t curr_thd_id,RaceEventType type,
		PStmt *pstmt);
	void HistoryDetection(Meta *meta,PStmt *curr_pstmt,Inst *inst,
		thread_t curr_thd_id,RaceEventType type,SyncObject *sync_obj);
private:
	TLS_KEY tls_key_;
	int prl_vrf_num_;
	VerifyRequestQueue vrf_req_que_;
	HtyDtcReqQueueTable htydtc_reqque_table_;
	std::tr1::unordered_map<thread_t,uint32> thd_uid_map_;
	
	DISALLOW_COPY_CONSTRUCTORS(ParallelVerifierMl);
};

}
#endif //__PARALLEL_VERIFIER_ML_H