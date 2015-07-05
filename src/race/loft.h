#ifndef  __RACE_LOFT_H
#define __RACE_LOFT_H

//lock-optimized fast track
#include "race/fast_track.h"

namespace race {

class Loft:public FastTrack {
public:
	typedef std::map<thread_t,MutexMeta *> ThreadMutexMap;
	Loft() {}
	~Loft() {}

	void Register();
	bool Enabled();
	void Setup(Mutex *lock,RaceDB *race_db);

	void AfterPthreadMutexLock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
	void BeforePthreadMutexUnlock(thread_t curr_thd_id,timestamp_t curr_thd_clk, 
		Inst *inst,address_t addr);
protected:

	class LoftMutexMeta:public RwlockMeta {
	public:
		LoftMutexMeta() {}
		~LoftMutexMeta() {}
		//last released thread
		thread_t lastrld_thd_id;
	};

	MutexMeta *GetMutexMeta(address_t addr);
	void ProcessLock(thread_t curr_thd_id,MutexMeta *meta);
	void ProcessUnlock(thread_t curr_thd_id,MutexMeta *meta);

	//thread and last released lock mapping
	ThreadMutexMap thread_lastrldlock_map_;
private:
	DISALLOW_COPY_CONSTRUCTORS(Loft);
};

} //namespace race

#endif