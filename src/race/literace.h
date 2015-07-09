#ifndef __RACE_LITERACE_H
#define __RACE_LITERACE_H

/**
 * LiteRace use the pure happens-before as well as sampling
 */
#include "race/djit.h"

namespace race {

class LiteRace:public Djit {
public:
	LiteRace() {}
	~LiteRace() {}

	void Register();
	bool Enabled();
	void Setup(Mutex *lock,RaceDB *race_db);

protected:
	typedef enum {
		ADAPTIVE,FIX
	} SampType;

	class LrMeta:public DjitMeta {
	public:
		explicit LrMeta(address_t a):DjitMeta(a) {
			icount=ncount=0;
			scount=100;
			rate=1000;
		}
		~LrMeta() {}
		uint64 icount; //instruments counts
		uint64 ncount; //non sampling counts
		uint32 scount; //sampling counts
		uint32 rate;  //sampling rate
	};

	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst);
	//do nothing for ProcessFree

	SampType samp_type_;
	uint32 rate_param_;

private:
	DISALLOW_COPY_CONSTRUCTORS(LiteRace);
	void UpdateRate(LrMeta *meta);
};

} //namespace race

#endif /* __RACE_LITERACE_H */