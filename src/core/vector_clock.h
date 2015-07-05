#ifndef __CORE_VECTOR_CLOCK_H
#define __CORE_VECTOR_CLOCK_H

#include "core/basictypes.h"
#include <map>

//accordion vector clock
class VectorClock {
public:
	VectorClock() {}
	~VectorClock() {}

	static volatile uint32 vc_joins_;
	static volatile uint32 vc_assigns_;
	static volatile uint32 vc_hb_cmps_;

	bool HappensBefore(VectorClock *vc);
	bool HappensAfter(VectorClock *vc);

	bool PreciseHappensBefore(VectorClock *vc);
	
	void Join(VectorClock *vc);
	void Increment(thread_t thdId);
	timestamp_t GetClock(thread_t thdId);
	void SetClock(thread_t thdId,timestamp_t clk);
	bool Equal(VectorClock *vc);
	bool Find(thread_t thdId) {
		return map_.find(thdId)!=map_.end();
	}
	size_t Size() {
		return map_.size();
	}
	void Erase(thread_t thdId) {
		map_.erase(thdId);
	}
	void Clear() {
		map_.clear();
	}
	std::string ToString();
	//vector clock iterate for traversal
	void IterBegin() {it_=map_.begin();}
	bool IterEnd() { return it_==map_.end(); }
	void IterNext() {++it_;}
	thread_t IterCurrThd() {return it_->first;}
	timestamp_t IterCurrClk(){return it_->second;}
	VectorClock & operator =(const VectorClock &vc) { 
		map_=vc.map_;
		vc_assigns_++;
		return *this; 
	}
	uint32 GetMemSize() {
		return sizeof(ThreadClockMap::value_type)*map_.size();
	}
protected:
	typedef std::map<thread_t,timestamp_t> ThreadClockMap;
	ThreadClockMap map_;
	ThreadClockMap::iterator it_;

	//using default copy constructor
};

#endif