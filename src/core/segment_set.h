#ifndef __CORE_SEGMENT_SET_H
#define __CORE_SEGMENT_SET_H

#include "core/basictypes.h"
#include "core/vector_clock.h"
#include "core/lock_set.h"
#include "core/log.h"
#include <map>

class SegmentSet;

class Segment {
public:
	typedef uint32 signature_t;

	Segment(thread_t tid,VectorClock &vector_clock);
	Segment(thread_t tid,LockSet *writer_lock_set,LockSet *reader_lock_set,
		VectorClock &vector_clock);
	~Segment() {}

	LockSet *GetWriterLockSet() { return &writer_lock_set_; }
	LockSet *GetReaderLockSet() { return &reader_lock_set_; }
	thread_t GetThreadId() { return thread_id_; }
	VectorClock *GetVectorClock() { return &vector_clock_; }
	signature_t GetSignature() { return signature_; }

	void UpReference() { reference_count_++; }
	void DownReference() { reference_count_--; }
	bool InActivate() {
		DEBUG_ASSERT(reference_count_>=0);
		return reference_count_==0;
	}

	bool PartHappensBefore(Segment *segment) {
		VectorClock *vc=segment->GetVectorClock();
		thread_t tid=segment->GetThreadId();
		//0 indicates that segment's vc corresponding entry is null
		// if(vector_clock_.GetClock(thread_id_)==0 || vc->GetClock(thread_id_)==0
		// 	|| vector_clock_.GetClock(tid)==0 || vc->GetClock(tid)==0 )
		// 	return false;
		return ( vector_clock_.GetClock(thread_id_)<=vc->GetClock(thread_id_) &&
		vector_clock_.GetClock(tid)<vc->GetClock(tid) );
	}
protected:
	//static signature
	static signature_t signature;
	//each segment's signature
	signature_t signature_;
	thread_t thread_id_;
	LockSet writer_lock_set_;
	LockSet reader_lock_set_;
	VectorClock vector_clock_;

	uint64 reference_count_;

private:
	friend class SegmentSet;
};

// class SegmentSet {
// public:
// 	typedef std::map<signature_t,Segment *> SignSegmentMap;
// 	SegmentSet() {}
// 	~SegmentSet();

// 	Segment *Add(thread_t tid,VectorClock &vector_clock);
// 	Segment *Add(thread_t tid,LockSet &writer_lock_set,LockSet &reader_lock_set,
// 		VectorClock &vector_clock);
// 	bool Exist(Segment *segment) ;
// 	Segment *Find(Segment *segment) ;

// 	// void IterBegin() {it_=sign_segment_map_.begin();}
// 	// bool IterEnd() {return it_==sign_segment_map_.end();}
// 	// void IterNext() {++it_;}
// 	// Segment *CurrIterSegment() { return it->second; }
// 	// signature_t *CurrIterSignature() { return it->first; }
// 	void RemoveHappensBeforeSegments(Segment *segment);
// protected:
// 	SignSegmentMap sign_segment_map_;
// 	// SignSegmentMap::iterator it;
// private:
// 	DISALLOW_COPY_CONSTRUCTORS(SegmentSet);
// };

#endif /* __CORE_SEGMENT_SET_H */