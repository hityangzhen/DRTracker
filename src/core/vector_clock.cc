#include "core/vector_clock.h"
#include <sstream>

volatile uint32 VectorClock::vc_joins_=0;
volatile uint32 VectorClock::vc_assigns_=0;
volatile uint32 VectorClock::vc_hb_cmps_=0;

#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

bool VectorClock::HappensBefore(VectorClock *vc)
{
	vc_hb_cmps_++;
	//define two iterators for sorted comparasion
	ThreadClockMap::iterator curr_it=map_.begin();
	ThreadClockMap::iterator vc_it=vc->map_.begin();
	for(;curr_it!=map_.end();++curr_it) {
		thread_t currThdId=curr_it->first;
		timestamp_t currThdClk=curr_it->second;

		bool valid=false;
		//compare same thread's different clock in different vc
		//ensure current thread vc's entry not greater than vc's entry
		for(;vc_it!=vc->map_.end();vc_it++) {
			thread_t vcThdId=vc_it->first;
			timestamp_t vcThdClk=vc_it->second;

			if(vcThdId==currThdId) {
				if(vcThdClk>=currThdClk) {
					valid=true;
					++vc_it;
				}
				break;
			}
			//map is default sorted by the key
			else if(vcThdId>currThdId) {
				break;
			}
		}
		if(!valid)
			return false;
	}
	return true;
}

bool VectorClock::PreciseHappensBefore(VectorClock *vc)
{
	vc_hb_cmps_++;
	//define two iterators for sorted comparasion
	ThreadClockMap::iterator curr_it=map_.begin();
	ThreadClockMap::iterator vc_it=vc->map_.begin();
	bool precise=false;
	for(;curr_it!=map_.end();++curr_it) {
		thread_t currThdId=curr_it->first;
		timestamp_t currThdClk=curr_it->second;

		bool valid=false;
		//compare same thread's different clock in different vc
		//ensure current thread vc's entry not greater than vc's entry
		for(;vc_it!=vc->map_.end();vc_it++) {
			thread_t vcThdId=vc_it->first;
			timestamp_t vcThdClk=vc_it->second;

			if(vcThdId==currThdId) {
				if(vcThdClk>=currThdClk) {
					if(vcThdClk>currThdClk)
						precise=true;
					valid=true;
					++vc_it;
				}
				break;
			}
			//map is default sorted by the key
			else if(vcThdId>currThdId) {
				break;
			}
		}
		if(!valid)
			return false;
	}
	return precise;
}

bool VectorClock::HappensAfter(VectorClock *vc)
{
	vc_hb_cmps_++;
	//define two iterators for sorted comparasion
	ThreadClockMap::iterator curr_it=map_.begin();
	ThreadClockMap::iterator vc_it=vc->map_.begin();
	for(;curr_it!=map_.end();++curr_it) {
		thread_t currThdId=curr_it->first;
		timestamp_t currThdClk=curr_it->second;

		bool valid=false;
		//compare same thread's different clock in different vc
		//ensure vc entry not greater than current thread vc's entry
		for(;vc_it!=vc->map_.end();vc_it++) {
			thread_t vcThdId=vc_it->first;
			timestamp_t vcThdClk=vc_it->second;

			if(vcThdId==currThdId) {
				if(vcThdClk <= currThdClk) {
					valid=true;
					++vc_it;
				}
				break;
			}
			else if(vcThdId>currThdId) {
				break;
			}
		}
		if(!valid)
			return false;
	}
	return true;
}

//Merge two vector clock
void VectorClock::Join(VectorClock *vc)
{
	for(ThreadClockMap::iterator it=vc->map_.begin();
		it!=vc->map_.end();it++) {
		ThreadClockMap::iterator mit=map_.find(it->first);
		if(mit==map_.end())
			map_[it->first]=it->second;
		else
			map_[it->first]=MAX(it->second,mit->second);
	}
	vc_joins_++;
}

void VectorClock::Increment(thread_t thdId) 
{
	ThreadClockMap::iterator it=map_.find(thdId);
	if(it==map_.end())
		map_[thdId]=1;
	else
		it->second++;
}

timestamp_t VectorClock::GetClock(thread_t thdId) 
{
	ThreadClockMap::iterator it=map_.find(thdId);
	if(it==map_.end())
		return 0;
	return it->second;
}

void VectorClock::SetClock(thread_t thdId,timestamp_t clk)
{
	map_[thdId]=clk;
}

bool VectorClock::Equal(VectorClock *vc)
{
	if(map_.size()!=vc->map_.size())
		return false;
	ThreadClockMap::iterator curr_it=map_.begin();
	ThreadClockMap::iterator vc_it=vc->map_.begin();

	for(;curr_it!=map_.end() && vc_it != vc->map_.end();++curr_it,vc_it++) {
		if(curr_it->first!=vc_it->first || curr_it->second!=vc_it->second)
			return false;
	}
	return true;
}
//for debug
std::string VectorClock::ToString()
{
	std::stringstream ss;
	ss<<"[";
	for(ThreadClockMap::iterator it=map_.begin();it!=map_.end();it++)
		ss<<"T"<<std::hex<<it->first<<":"<<std::dec<<it->second<<" ";
	ss<<"]";

	return ss.str();
}