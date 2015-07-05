#include "race/thread_sanitizer.h"
#include "core/log.h"

namespace race {

ThreadSanitizer::ThreadSanitizer():track_racy_inst_(false)
{}

ThreadSanitizer::~ThreadSanitizer()
{
	for(SegmentTable::iterator it=segment_table_.begin();
		it!=segment_table_.end();it++)
		expired_segment_set_.insert(it->second);

	for(SegmentSet::iterator it=expired_segment_set_.begin();
		it!=expired_segment_set_.end();it++)
		delete *it;
}

void ThreadSanitizer::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_thread_sanitizer",
		"whether enable the thread_sanitizer data race detector","0");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");
}

bool ThreadSanitizer::Enabled()
{
	return knob_->ValueBool("enable_thread_sanitizer");
}

void ThreadSanitizer::Setup(Mutex *lock,RaceDB *race_db)
{
	Detector::Setup(lock,race_db);
	track_racy_inst_=knob_->ValueBool("track_racy_inst");
}

void ThreadSanitizer::ThreadStart(thread_t curr_thd_id,thread_t parent_thd_id)
{
	Detector::ThreadStart(curr_thd_id,parent_thd_id);
	create_segment_map_[curr_thd_id]=true;
	
}

void ThreadSanitizer::AfterPthreadCreate(thread_t currThdId,timestamp_t currThdClk,
  								Inst *inst,thread_t childThdId)
{
	Detector::AfterPthreadCreate(currThdId,currThdClk,inst,childThdId);
	create_segment_map_[currThdId]=true;
}

void ThreadSanitizer::AfterPthreadJoin(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
								Inst *inst,thread_t child_thd_id)
{
	Detector::AfterPthreadJoin(curr_thd_id,curr_thd_clk,inst,child_thd_id);
	create_segment_map_[curr_thd_id]=true;
}

Detector::Meta * ThreadSanitizer::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new ThreadSanitizerMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void ThreadSanitizer::AfterPthreadMutexLock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);

	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);
	//we only synchronize procedure segments
	SyncEvent(curr_thd_id);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::BeforePthreadMutexUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//
	curr_writer_lock_set->Remove(addr);
	curr_reader_lock_set->Remove(addr);
	SyncEvent(curr_thd_id);
	create_segment_map_[curr_thd_id]=true;	
}


void ThreadSanitizer::AfterPthreadRwlockRdlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_reader_lock_set && curr_writer_lock_set);
	//reader lock can only protect read access
	curr_reader_lock_set->Add(addr);
	//if there have been sync operation
	if(lock_epoch_map_[addr].size()>0) {
		const Epoch &epoch=lock_epoch_map_[addr][0];
		curr_vc_map_[curr_thd_id]->SetClock(epoch.first,epoch.second);
	}
	//reader lock reference counting 
	if(lock_reference_map_.find(addr)!=lock_reference_map_.end())
		lock_reference_map_[addr]+=1;
	else
		lock_reference_map_[addr]=1;

	SyncEvent(curr_thd_id);
	create_segment_map_[curr_thd_id]=true;	
}

void ThreadSanitizer::AfterPthreadRwlockWrlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);

	LockSet *curr_writer_lock_set=GetWriterLockSet(curr_thd_id);
	LockSet *curr_reader_lock_set=GetReaderLockSet(curr_thd_id);
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);
	//writer lock can both protect write and read access
	curr_reader_lock_set->Add(addr);
	curr_writer_lock_set->Add(addr);

	//if there have been sync operation
	if(lock_epoch_map_[addr].size()>0) {
		std::vector<Epoch> &epoch_vector=lock_epoch_map_[addr];
		for(std::vector<Epoch>::iterator it=epoch_vector.begin()
			;it<epoch_vector.end();it++) {
			curr_vc_map_[curr_thd_id]->SetClock(it->first,it->second);
		}
	}
	//writer lock refenrence counting
	lock_reference_map_[addr]=1;

	SyncEvent(curr_thd_id);

	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::BeforePthreadRwlockUnlock(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	LockCountIncrease();
	ScopedLock lock(internal_lock_);
	LockSet *curr_writer_lock_set=writer_lock_set_table_[curr_thd_id];
	LockSet *curr_reader_lock_set=reader_lock_set_table_[curr_thd_id];
	DEBUG_ASSERT(curr_writer_lock_set && curr_reader_lock_set);

	DEBUG_ASSERT(lock_reference_map_[addr]>0);
	if(lock_reference_map_[addr]==1) {
		curr_writer_lock_set->Remove(addr);
		curr_reader_lock_set->Remove(addr);	
	}
	lock_reference_map_[addr]-=1;
	
	//synchronize the next get lock
	if(lock_reference_map_[addr]==0)
		lock_epoch_map_[addr].clear();
	SyncEvent(curr_thd_id);

	lock_epoch_map_[addr].push_back(std::make_pair(curr_thd_id,
		curr_vc_map_[curr_thd_id]->GetClock(curr_thd_id)));

	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::BeforePthreadCondSignal(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::BeforePthreadCondSignal(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;
}
   
void ThreadSanitizer::BeforePthreadCondBroadcast(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst, address_t addr)
{
	Detector::BeforePthreadCondBroadcast(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;	
}

void ThreadSanitizer::BeforePthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	//condition varibale waiting process first release lock and then block
	//this process is atomic so create segment only after the last 
	//synchronization
	Detector::BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,
		mutex_addr);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::AfterPthreadCondWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	Detector::AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,
		mutex_addr);
	create_segment_map_[curr_thd_id]=true;
}	

void ThreadSanitizer::BeforePthreadCondTimedwait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	BeforePthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void ThreadSanitizer::AfterPthreadCondTimedwait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk,Inst *inst,address_t cond_addr,address_t mutex_addr)
{
	AfterPthreadCondWait(curr_thd_id,curr_thd_clk,inst,cond_addr,mutex_addr);
}

void ThreadSanitizer::BeforePthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::BeforePthreadBarrierWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::AfterPthreadBarrierWait(thread_t curr_thd_id,
	timestamp_t curr_thd_clk, Inst *inst,address_t addr)
{
	Detector::AfterPthreadBarrierWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::BeforeSemPost(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr)
{
	Detector::BeforeSemPost(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::AfterSemWait(thread_t curr_thd_id,timestamp_t curr_thd_clk,
      Inst *inst,address_t addr)
{
	Detector::AfterSemWait(curr_thd_id,curr_thd_clk,inst,addr);
	create_segment_map_[curr_thd_id]=true;
}

void ThreadSanitizer::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{

	ThreadSanitizerMeta *thd_sani_meta=dynamic_cast<ThreadSanitizerMeta *>(meta);
	DEBUG_ASSERT(thd_sani_meta);

	Segment *segment=GetSegment(curr_thd_id);
	
	//ignore not the first read access in a same segment
	if(thd_sani_meta->segment_read_access_set.find(SegmentSign(segment))
		==thd_sani_meta->segment_read_access_set.end()) {
		thd_sani_meta->segment_read_access_set.insert(SegmentSign(segment));
	}
	else
		return ;

		
	// remove read segment sets happens-before current segment and
	// insert to expired segment set
	for(SegmentSet::iterator it=thd_sani_meta->reader_segment_set.begin();
		it!=thd_sani_meta->reader_segment_set.end();) {
		if((*it)->PartHappensBefore(segment)) {
			// INFO_FMT_PRINT("process read-remove read segment:0x%lx\n",
			// 	thd_sani_meta->addr);
			// INFO_FMT_PRINT("prior read segment set vc:%s\n",
			// (*it)->GetVectorClock()->ToString().c_str());
			// INFO_FMT_PRINT("current read segment set vc:%s\n",
			// segment->GetVectorClock()->ToString().c_str());

			expired_segment_set_.insert(*it);
			thd_sani_meta->reader_inst_table.erase(SegmentSign(*it));
			//dereference from the reader segment set
			(*it)->DownReference();

			ThreadSanitizerMeta::signature_t sig=SegmentSign(*it);
			if(thd_sani_meta->segment_read_access_set.find(sig)!=
				thd_sani_meta->segment_read_access_set.end())
				thd_sani_meta->segment_read_access_set.erase(sig);

			thd_sani_meta->reader_segment_set.erase(it++);
		}
		else
			it++;
	}

	segment->UpReference();
	thd_sani_meta->reader_segment_set.insert(segment);
	thd_sani_meta->reader_inst_table[SegmentSign(segment)]=inst;

	Racy(thd_sani_meta,inst);

	if(track_racy_inst_)
		thd_sani_meta->race_inst_set.insert(inst);	
}

void ThreadSanitizer::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	ThreadSanitizerMeta *thd_sani_meta=dynamic_cast<ThreadSanitizerMeta *>(meta);
	DEBUG_ASSERT(thd_sani_meta);

	Segment *segment=GetSegment(curr_thd_id);

	//ignore not the first write access in a same segment
	if(thd_sani_meta->segment_write_access_set.find(SegmentSign(segment))
		==thd_sani_meta->segment_write_access_set.end()) {
		thd_sani_meta->segment_write_access_set.insert(SegmentSign(segment));
	}
	else
		return ;

	//remove write and read segment sets happens-before current segment
	//and insert to expired segment set
	for(SegmentSet::iterator it=thd_sani_meta->reader_segment_set.begin();
		it!=thd_sani_meta->reader_segment_set.end();) {
		if((*it)->PartHappensBefore(segment)) {

			// INFO_FMT_PRINT("process write-remove read segment:0x%lx\n",
			// 	thd_sani_meta->addr);

			expired_segment_set_.insert(*it);
			thd_sani_meta->reader_inst_table.erase(SegmentSign(*it));
			//deference from the reader segment set
			(*it)->DownReference();

			ThreadSanitizerMeta::signature_t sig=SegmentSign(*it);
			if(thd_sani_meta->segment_read_access_set.find(sig)!=
				thd_sani_meta->segment_read_access_set.end())
				thd_sani_meta->segment_read_access_set.erase(sig);

			thd_sani_meta->reader_segment_set.erase(it++);

		}

		else
			it++;
	}

	for(SegmentSet::iterator it=thd_sani_meta->writer_segment_set.begin();
		it!=thd_sani_meta->writer_segment_set.end();) {
		if((*it)->PartHappensBefore(segment)) {

			// INFO_FMT_PRINT("process write-remove write segment:0x%lx\n",
			// 	thd_sani_meta->addr);

			expired_segment_set_.insert(*it);
			thd_sani_meta->writer_inst_table.erase(SegmentSign(*it));
			//deference from the writer segment set
			(*it)->DownReference();

			ThreadSanitizerMeta::signature_t sig=SegmentSign(*it);
			if(thd_sani_meta->segment_write_access_set.find(sig)!=
				thd_sani_meta->segment_write_access_set.end())
				thd_sani_meta->segment_write_access_set.erase(sig);

			thd_sani_meta->writer_segment_set.erase(it++);
		}
		else
			it++;
	}

	segment->UpReference();
	thd_sani_meta->writer_segment_set.insert(segment);
	thd_sani_meta->writer_inst_table[SegmentSign(segment)]=inst;

	Racy(thd_sani_meta,inst);

	if(track_racy_inst_)
		thd_sani_meta->race_inst_set.insert(inst);
}

void ThreadSanitizer::ProcessFree(Meta *meta)
{
	ThreadSanitizerMeta *thd_sani_meta=dynamic_cast<ThreadSanitizerMeta *>(meta);
	DEBUG_ASSERT(thd_sani_meta);

	if(track_racy_inst_ && thd_sani_meta->racy) {
		for(ThreadSanitizerMeta::InstSet::iterator it=thd_sani_meta->race_inst_set.begin();
			it!=thd_sani_meta->race_inst_set.end();it++) {
			race_db_->SetRacyInst(*it,true);
		}
	}

	//dereference the reader and writer segment set
	//compiler for clear the segment set
	for(SegmentSet::iterator it=thd_sani_meta->reader_segment_set.begin();
		it!=thd_sani_meta->reader_segment_set.end();it++) {
		(*it)->DownReference();
		expired_segment_set_.insert(*it);

	}
	for(SegmentSet::iterator it=thd_sani_meta->writer_segment_set.begin();
		it!=thd_sani_meta->writer_segment_set.end();it++) {
		(*it)->DownReference();
		expired_segment_set_.insert(*it);
	}
}

//check concurrent pairs
void ThreadSanitizer::Racy(ThreadSanitizer::ThreadSanitizerMeta *thd_sani_meta,Inst *inst)
{
	
	SegmentSet::iterator rd_it=thd_sani_meta->reader_segment_set.begin();
	SegmentSet::iterator wr_it=thd_sani_meta->writer_segment_set.begin();

	SegmentSet::iterator rd_it_end=thd_sani_meta->reader_segment_set.end();
	SegmentSet::iterator wr_it_end=thd_sani_meta->writer_segment_set.end();
	// INFO_FMT_PRINT("racy:%lx\n",thd_sani_meta->addr);
	// INFO_FMT_PRINT("write segment set size:%ld,read segment set size:%ld\n",
	// 	thd_sani_meta->writer_segment_set.size(),
	// 	thd_sani_meta->reader_segment_set.size());
	// for(SegmentSet::iterator it=thd_sani_meta->writer_segment_set.begin();
	// 	it!=thd_sani_meta->writer_segment_set.end();it++) {
	// 	INFO_FMT_PRINT("write segment set vc:%s\n",
	// 		(*it)->GetVectorClock()->ToString().c_str());
	// }
	// for(SegmentSet::iterator it=thd_sani_meta->reader_segment_set.begin();
	// 	it!=thd_sani_meta->reader_segment_set.end();it++) {
	// 	INFO_FMT_PRINT("read segment set vc:%s\n",
	// 		(*it)->GetVectorClock()->ToString().c_str());
	// }

	LockSet *curr_writer_lock_set;
	Inst *curr_writer_inst;
	thread_t curr_thd_id;
	//iterate segment pairs
	for(;wr_it!=wr_it_end;wr_it++) {

		curr_writer_lock_set=(*wr_it)->GetWriterLockSet();
		curr_writer_inst=thd_sani_meta->writer_inst_table[SegmentSign(*wr_it)];
		curr_thd_id=(*wr_it)->GetThreadId();
		//check all write-write pairs
		SegmentSet::iterator inner_wr_it=wr_it;
		inner_wr_it++;
		for(;inner_wr_it!=wr_it_end;inner_wr_it++) {
			//
			if(curr_thd_id!=(*inner_wr_it)->GetThreadId() && 
				curr_writer_lock_set->Disjoint((*inner_wr_it)->GetWriterLockSet())) {

				//PrintDebugRaceInfo("THREADSANITIZER",WRITETOWRITE,thd_sani_meta,curr_thd_id,inst);

				thd_sani_meta->racy=true;
				DEBUG_ASSERT(thd_sani_meta->writer_inst_table.find(SegmentSign(*inner_wr_it))!=
					thd_sani_meta->writer_inst_table.end());
				Inst *writer_inst=thd_sani_meta->writer_inst_table[SegmentSign(*inner_wr_it)];
				ReportRace(thd_sani_meta,(*inner_wr_it)->GetThreadId(),writer_inst,RACE_EVENT_WRITE,
					curr_thd_id,curr_writer_inst,RACE_EVENT_WRITE);
			}
		}
		//check all write-read pairs
		for(;rd_it!=rd_it_end;rd_it++) {

			if(curr_thd_id!=(*rd_it)->GetThreadId() && 
				!(*wr_it)->PartHappensBefore(*rd_it) && 
				curr_writer_lock_set->Disjoint((*rd_it)->GetReaderLockSet())) {

				//PrintDebugRaceInfo("THREADSANITIZER",WRITETOREAD_OR_READTOWRITE,thd_sani_meta,
				//curr_thd_id,inst);

				thd_sani_meta->racy=true;
				DEBUG_ASSERT(thd_sani_meta->reader_inst_table.find(SegmentSign(*rd_it))!=
					thd_sani_meta->reader_inst_table.end());
				Inst *reader_inst=thd_sani_meta->reader_inst_table[SegmentSign(*rd_it)];
				ReportRace(thd_sani_meta,(*rd_it)->GetThreadId(),reader_inst,RACE_EVENT_READ,
					curr_thd_id,curr_writer_inst,RACE_EVENT_WRITE);	
			}
		}
	}
}

inline ThreadSanitizer::ThreadSanitizerMeta::signature_t 
ThreadSanitizer::SegmentSign(Segment *segment)
{
	return (((ThreadSanitizerMeta::signature_t)(segment))<<32)+segment->GetSignature();
}

void ThreadSanitizer::ClearExpiredSegments()
{
	SegmentSet::iterator it=expired_segment_set_.begin();
	for(;it!=expired_segment_set_.end();) {
		if((*it)->InActivate()) {
			delete *it;
			expired_segment_set_.erase(it++);
		}
		else
			it++;
	}
}

Segment *ThreadSanitizer::GetSegment(thread_t curr_thd_id)
{
	
	Segment *segment=NULL;
	Segment *old_segment=NULL;
	ClearExpiredSegments();
	if(create_segment_map_[curr_thd_id]) {
		segment=new Segment(curr_thd_id,writer_lock_set_table_[curr_thd_id],
			reader_lock_set_table_[curr_thd_id],*curr_vc_map_[curr_thd_id]);
		//waiting for next new segment
		create_segment_map_[curr_thd_id]=false;
		segment->UpReference();
		//
		if(segment_table_[curr_thd_id]) {
			old_segment=segment_table_[curr_thd_id];
			old_segment->DownReference();
			expired_segment_set_.insert(old_segment);
		}
		segment_table_[curr_thd_id]=segment;
	}
	else
		segment=segment_table_[curr_thd_id];
	return segment;
}

LockSet *ThreadSanitizer::GetWriterLockSet(thread_t curr_thd_id)
{
	LockSet *curr_lock_set=NULL;
	LockSetTable::iterator it;
	if((it=writer_lock_set_table_.find(curr_thd_id))==writer_lock_set_table_.end()
		|| writer_lock_set_table_[curr_thd_id]==NULL) {
		curr_lock_set=new LockSet;
		writer_lock_set_table_[curr_thd_id]=curr_lock_set;
	}
	else
		curr_lock_set=it->second;
	return curr_lock_set;
}


LockSet *ThreadSanitizer::GetReaderLockSet(thread_t curr_thd_id)
{
	LockSet *curr_lock_set=NULL;
	LockSetTable::iterator it;
	if((it=reader_lock_set_table_.find(curr_thd_id))==reader_lock_set_table_.end()
		|| reader_lock_set_table_[curr_thd_id]==NULL) {
		curr_lock_set=new LockSet;
		reader_lock_set_table_[curr_thd_id]=curr_lock_set;
	}
	else
		curr_lock_set=it->second;
	return curr_lock_set;
}

} //namespace race
