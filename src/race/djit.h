#ifndef __RACE_DJIT_H
#define __RACE_DJIT_H

#include <tr1/unordered_map>
#include "core/basictypes.h"
#include "core/vector_clock.h"
#include "race/detector.h"
#include "race/race.h"

//Djit algorithm

namespace race {

class Djit:public Detector {
public:
	Djit();
	~Djit();

	void Register();
	bool Enabled();
	void Setup(Mutex *lock,RaceDB *race_db);

protected:
	//the meta data for the memory access
	class DjitMeta:public Meta {
	public:	
		typedef std::map<thread_t,Inst *>InstMap;
		typedef std::set<Inst *>InstSet;

		explicit DjitMeta(address_t a):Meta(a),racy(false) {}
		~DjitMeta() {}

		bool racy; //whether this meta is involved in any race
		VectorClock writer_vc;
		InstMap writer_inst_table;
		VectorClock reader_vc;
		InstMap reader_inst_table;
		InstSet race_inst_set;
	};

	//override virtual function
	Meta *GetMeta(address_t iaddr);
	void ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst);
	void ProcessFree(Meta *meta);
	//whether to track the racy inst
	bool track_racy_inst_;
private:
	DISALLOW_COPY_CONSTRUCTORS(Djit);
};

} //namespace race

#endif