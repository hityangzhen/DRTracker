#ifndef __RACE_PROFILER_H
#define __RACE_PROFILER_H

#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "race/race.h"
#include "race/djit.h"
#include "race/eraser.h"
#include "race/race_track.h"
#include "race/helgrind.h"
#include "race/thread_sanitizer.h"
#include "race/fast_track.h"
#include "race/literace.h"
#include "race/loft.h"
#include "race/acculock.h"
#include "race/multilock_hb.h"
#include "race/simple_lock.h"
#include "race/simplelock_plus.h"
#include "race/potential_race.h"
#include "race/verifier.h"
#include "race/verifier_sl.h"
#include "race/verifier_ml.h"
#include "race/pre_group.h"
#include "race/parallel_verifier_ml.h"

namespace race{

class Profiler:public ExecutionControl {
public:
	Profiler():race_db_(NULL),
			race_rp_(NULL),
			djit_analyzer_(NULL),
			eraser_analyzer_(NULL),
			race_track_analyzer_(NULL),
			helgrind_analyzer_(NULL),
			thread_sanitizer_analyzer_(NULL),
			fast_track_analyzer_(NULL),
			literace_analyzer_(NULL),
			loft_analyzer_(NULL),
			acculock_analyzer_(NULL),
			multilock_hb_analyzer_(NULL),
			simple_lock_analyzer_(NULL),
			simplelock_plus_analyzer_(NULL),
			prace_db_(NULL),
			verifier_analyzer_(NULL),
			verifier_sl_analyzer_(NULL),
			verifier_ml_analyzer_(NULL),
			pre_group_analyzer_(NULL),
			prl_vrf_ml_analyzer_(NULL),
			exit_num_(0),exit_flag_(0),unit_size_(0)
	{}
	~Profiler() {}
protected:
	void HandlePreSetup();
	void HandlePostSetup();
	bool HandleIgnoreMemAccess(IMG img);
	void HandleProgramExit();
	void HandleCreateDetectionThread(thread_t thd_id);
	void HandleCreateVerificationThread(thread_t thd_id);
	address_t GetUnitSize() { return unit_size_; }
	//======================data race detection=====================
	RaceDB *race_db_;
	RaceReport *race_rp_;
	Djit *djit_analyzer_;
	Eraser *eraser_analyzer_;
	RaceTrack *race_track_analyzer_;
	Helgrind *helgrind_analyzer_;
	ThreadSanitizer *thread_sanitizer_analyzer_;
	FastTrack *fast_track_analyzer_;
	LiteRace *literace_analyzer_;
	Loft *loft_analyzer_;
	AccuLock *acculock_analyzer_;
	MultiLockHb *multilock_hb_analyzer_;
	SimpleLock *simple_lock_analyzer_;
	SimpleLockPlus *simplelock_plus_analyzer_;
	//==============================end============================

	//======================data race verifier=====================
	PRaceDB *prace_db_;
	Verifier *verifier_analyzer_;
	VerifierSl *verifier_sl_analyzer_;
	VerifierMl *verifier_ml_analyzer_;
	PreGroup *pre_group_analyzer_;
	ParallelVerifierMl *prl_vrf_ml_analyzer_;
	//==============================end============================	
	volatile size_t exit_num_;
	volatile unsigned exit_flag_;
	address_t unit_size_;
private:
	void LoadPStmts();
	void LoadPStmts2();
	void StartWaitVerification();
	void StartHistoryDetection(thread_t thd_id);
	DISALLOW_COPY_CONSTRUCTORS(Profiler);
};
} //namespace race

#endif