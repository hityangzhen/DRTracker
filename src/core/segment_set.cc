#include "core/segment_set.h"
#include "core/log.h"

Segment::signature_t Segment::signature=0;

Segment::Segment(thread_t tid,LockSet *writer_lock_set,LockSet *reader_lock_set,
	VectorClock &vector_clock):thread_id_(tid),vector_clock_(vector_clock)
{
	if(writer_lock_set)
		writer_lock_set_=*writer_lock_set;
	if(reader_lock_set)
		reader_lock_set_=*reader_lock_set;
	reference_count_=0;
	signature_=++signature;
}

Segment::Segment(thread_t tid,VectorClock &vector_clock)
	:thread_id_(tid),vector_clock_(vector_clock)
{
	reference_count_=0;
	signature_=++signature;
}

// SegmentSet::~SegmentSet()
// {
// 	for(SignSegmentMap::iterator it=sign_segment_map_.begin();
// 		it!=sign_segment_map_.end();it++) {
// 		delete it->second;
// 	}
// }

// Segment *SegmentSet::Add(thread_t tid,VectorClock &vector_clock)
// {
// 	Segment *segment=new Segment(tid,vector_clock);
// 	DEBUG_ASSERT(segment);
// 	sign_segment_map_.[segment->signature_]=segment;
// 	return segment;
// }

// Segment *SegmentSet::Add(thread_t tid,LockSet &writer_lock_set,LockSet &reader_lock_set,
// 		VectorClock &vector_clock)
// {
// 	SegmentSet *segment=new SegmentSet(tid,writer_lock_set,reader_lock_set,
// 		vector_clock);
// 	DEBUG_ASSERT(segment);
// 	sign_segment_map_.[segment->signature_]=segment;
// 	return segment;	
// }

// bool SegmentSet::Exist(Segment *segment)
// {
// 	SignSegmentMap::iterator it=sign_segment_map_.find(segment->signature_);
// 	if(it!=sign_segment_map_.end())
// 		return true;
// 	return false;
// }
// Segment *SegmentSet::Find(Segment *segment)
// {
// 	SignSegmentMap::iterator it=sign_segment_map_.find(segment->signature_);
// 	if(it!=sign_segment_map_.end())
// 		return it->second;
// 	return NULL;
// }

// bool SegmentSet::RemoveHappensBeforeSegments(Segment *segment)
// {
// 	for(SignSegmentMap::iterator it=sign_segment_map_.begin();
// 		it!=sign_segment_map_.end();) {
// 		if(it->second->PartHappensBefore(segment))
// 			sign_segment_map_.erase(it++);
// 	}
// }


