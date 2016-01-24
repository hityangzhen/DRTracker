#include "race/profiler.h"
#include "core/log.h"

namespace race{
void Profiler::HandlePreSetup()
{
	ExecutionControl::HandlePreSetup();
	knob_->RegisterBool("ignore_lib","whether ignore accesses from common libraries","0");
	knob_->RegisterStr("race_in","the input race database path","race.db");
	knob_->RegisterStr("race_out","the output race database path","race.db");
	knob_->RegisterStr("race_report","the output race report path","race.rp");

	//======================data race detection=====================
	// djit_analyzer_=new Djit;
	// djit_analyzer_->Register();

	// eraser_analyzer_=new Eraser();
	// eraser_analyzer_->Register();

	// race_track_analyzer_=new RaceTrack();
	// race_track_analyzer_->Register();

	// helgrind_analyzer_=new Helgrind();
	// helgrind_analyzer_->Register();

	// thread_sanitizer_analyzer_=new ThreadSanitizer();
	// thread_sanitizer_analyzer_->Register();
	
	// fast_track_analyzer_=new FastTrack();
	// fast_track_analyzer_->Register();

	// literace_analyzer_=new LiteRace();
	// literace_analyzer_->Register();	

	// loft_analyzer_=new Loft();
	// loft_analyzer_->Register();

	// acculock_analyzer_=new AccuLock();
	// acculock_analyzer_->Register();

	multilock_hb_analyzer_=new MultiLockHb();
	multilock_hb_analyzer_->Register();

	// simple_lock_analyzer_=new SimpleLock();
	// simple_lock_analyzer_->Register();

	// simplelock_plus_analyzer_=new SimpleLockPlus();
	// simplelock_plus_analyzer_->Register();

	//==============================end============================

	//======================data race verifier=====================
	// verifier_analyzer_=new Verifier;
	// verifier_analyzer_->Register();

	// verifier_sl_analyzer_=new VerifierSl();
	// verifier_sl_analyzer_->Register();

	// verifier_ml_analyzer_=new VerifierMl();
	// verifier_ml_analyzer_->Register();

	// pre_group_analyzer_=new PreGroup();
	// pre_group_analyzer_->Register();	
	//==============================end============================	
}

void Profiler::HandlePostSetup()
{
	ExecutionControl::HandlePostSetup();
	//load race db
	race_db_=new RaceDB(CreateMutex());
	race_db_->Load(knob_->ValueStr("race_in"),sinfo_);
	//create race report
	race_rp_=new RaceReport(CreateMutex());
	//======================data race detection=====================

	// //add  data race detector
	// if(djit_analyzer_->Enabled()) {
	// 	djit_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(djit_analyzer_);
	// }

	// if(eraser_analyzer_->Enabled()) {
	// 	eraser_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(eraser_analyzer_);
	// }
	
	// if(race_track_analyzer_->Enabled()) {
	// 	race_track_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(race_track_analyzer_);
	// }	

	// if(helgrind_analyzer_->Enabled()) {
	// 	helgrind_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(helgrind_analyzer_);
	// }	

	// if(thread_sanitizer_analyzer_->Enabled()) {
	// 	thread_sanitizer_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(thread_sanitizer_analyzer_);
	// }

	// if(fast_track_analyzer_->Enabled()) {
	// 	fast_track_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(fast_track_analyzer_);
	// }

	// if(literace_analyzer_->Enabled()) {
	// 	literace_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(literace_analyzer_);
	// }	

	// if(loft_analyzer_->Enabled()) {
	// 	loft_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(loft_analyzer_);
	// }

	// if(acculock_analyzer_->Enabled()) {
	// 	acculock_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(acculock_analyzer_);
	// }

	if(multilock_hb_analyzer_->Enabled()) {
		multilock_hb_analyzer_->Setup(CreateMutex(),race_db_);
		AddAnalyzer(multilock_hb_analyzer_);
	}

	// if(simple_lock_analyzer_->Enabled()) {
	// 	simple_lock_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(simple_lock_analyzer_);
	// }

	// if(simplelock_plus_analyzer_->Enabled()) {
	// 	simplelock_plus_analyzer_->Setup(CreateMutex(),race_db_);
	// 	AddAnalyzer(simplelock_plus_analyzer_);
	// }
	//==============================end============================

	//======================data race verifier=====================
	// if(verifier_analyzer_->Enabled()) {
	// 	LoadPStmts();
	// 	verifier_analyzer_->Setup(CreateMutex(),CreateMutex(),race_db_,
	//		prace_db_);
	// 	AddAnalyzer(verifier_analyzer_);
	// }

	// if(verifier_sl_analyzer_->Enabled()) {
	// 	LoadPStmts();
	// 	verifier_sl_analyzer_->Setup(CreateMutex(),CreateMutex(),
	//		race_db_,prace_db_);
	// 	AddAnalyzer(verifier_sl_analyzer_);
	// }

	// if(verifier_ml_analyzer_->Enabled()) {
	// 	LoadPStmts();
	// 	verifier_ml_analyzer_->Setup(CreateMutex(),CreateMutex(),
	// 		race_db_,prace_db_);
	// 	AddAnalyzer(verifier_ml_analyzer_);
	// }

	// if(pre_group_analyzer_->Enabled()) {
	// 	LoadPStmts2();
	// 	pre_group_analyzer_->Setup(CreateMutex(),prace_db_);
	// 	AddAnalyzer(pre_group_analyzer_);
	// }
	//==============================end============================
}

bool Profiler::HandleIgnoreMemAccess(IMG img)
{
	if(!IMG_Valid(img))
		return true;
	Image *image=sinfo_->FindImage(IMG_Name(img));
	if(image->IsPthread())
		return true;
	if(knob_->ValueBool("ignore_lib"))
		if(image->IsCommonLib())
			return true;
	return false;
}

void Profiler::HandleProgramExit()
{
	ExecutionControl::HandleProgramExit();

	//======================data race detection=====================
	// //save statistics
	// djit_analyzer_->SaveStatistics("statistics");
	// //eraser_analyzer_->SaveStatistics("statistics");
	// //thread_sanitizer_analyzer_->SaveStatistics("statistics");
	// //helgrind_analyzer_->SaveStatistics("statistics");
	// //loft_analyzer_->SaveStatistics("statistics");
	// //fast_track_analyzer_->SaveStatistics("statistics");
	// //literace_analyzer_->SaveStatistics("statistics");
	// //acculock_analyzer_->SaveStatistics("statistics");
	// //multilock_hb_analyzer_->SaveStatistics("statistics");
	// //simple_lock_analyzer_->SaveStatistics("statistics");
	// //simplelock_plus_analyzer_->SaveStatistics("statistics");

	//save race db
	race_db_->Save(knob_->ValueStr("race_out"),sinfo_);

	//save race report
	race_rp_->Save(knob_->ValueStr("race_report"),race_db_);
	
	delete race_db_;
	delete race_rp_;
	//==============================end============================

	//======================data race verifier=====================
	// delete verifier_ml_analyzer_;
	// delete prace_db_;
	
	// pre_group_analyzer_->Export();
	// delete pre_group_analyzer_;
	// delete prace_db_;
	//==============================end============================	
}

void Profiler::LoadPStmts()
{
	//load the pstmts into prace_db
	prace_db_=new PRaceDB;
	char buffer[100];
	const char *delimit=" ", *fn=NULL, *l=NULL;
	PStmt *pstmt=NULL;
	for(std::vector<std::string>::iterator iter=
		static_profile_.begin();iter!=static_profile_.end();
		iter++) {
		iter->copy(buffer,iter->size(),0);
		buffer[iter->size()]='\0';
		fn=strtok(buffer,delimit);
		l=strtok(NULL,delimit);
		DEBUG_ASSERT(fn && l);
		pstmt=prace_db_->GetPStmtAndCreate(fn,l);
		fn=strtok(NULL,delimit);				
		l=strtok(NULL,delimit);
		DEBUG_ASSERT(fn && l);
		prace_db_->BuildRelationMapping(pstmt,fn,l);
	}
}

void Profiler::LoadPStmts2()
{
	//load the pstmts into prace_db
	prace_db_=new PRaceDB;
	char buffer[100];
	const char *delimit=" ", *fn=NULL, *l=NULL;
	PStmt *pstmt=NULL;
	for(std::vector<std::string>::iterator iter=
		static_profile_.begin();iter!=static_profile_.end();
		iter++) {
		iter->copy(buffer,iter->size(),0);
		buffer[iter->size()]='\0';
		fn=strtok(buffer,delimit);
		l=strtok(NULL,delimit);
		DEBUG_ASSERT(fn && l);
		pstmt=prace_db_->GetSortedPStmtAndCreate(fn,l);
		fn=strtok(NULL,delimit);				
		l=strtok(NULL,delimit);
		DEBUG_ASSERT(fn && l);
		prace_db_->BuildRelationMapping(pstmt,fn,l);
	}
}

}// namespace race
