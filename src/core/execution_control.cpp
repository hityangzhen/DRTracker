#include "core/execution_control.hpp"

#include "core/log.h"
#include "core/pin_util.h"

ExecutionControl *ExecutionControl::ctrl_=NULL;

ExecutionControl::ExecutionControl()
	:kernel_lock_(NULL),knob_(NULL),debug_file_(NULL),
	callstack_info_(NULL),debug_analyzer_(NULL),sinfo_(NULL),
	main_thread_started_(false),main_thd_id_(INVALID_THD_ID)
	{}

void ExecutionControl::Initialize()
{
	log_init(CreateMutex());
	Knob::Initialize(new PinKnob());
	kernel_lock_=CreateMutex();
	knob_=Knob::Get();
	ctrl_=this;
}

void ExecutionControl::PreSetup()
{
	knob_->RegisterStr("debug_out","the output file for the debug messages","stdout");
	knob_->RegisterStr("sinfo_in","the input static info database path","sinfo.db");
	knob_->RegisterStr("sinfo_out","the output static info database path","sinfo.db");

	knob_->RegisterBool("partial_instrument","whether instrument a part of the"
		" program or not","0");
	knob_->RegisterStr("static_profile","the potential race statement pairs generated"
		" by static race detector","0");
	knob_->RegisterStr("instrumented_lines","the instrumented lines traversed from"
		" static_profile","0");

	debug_analyzer_=new DebugAnalyzer;
	debug_analyzer_->Register();

	HandlePreSetup();
}

void ExecutionControl::PostSetup()
{
	//choose logtype and logfile
	if(knob_->ValueStr("debug_out").compare("stderr")==0) {
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(stderrLogFile);
	}
	else if(knob_->ValueStr("debug_out").compare("stdout")==0) {
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(stdoutLogFile);
	}
	else {
		debug_file_=new FileLogFile(knob_->ValueStr("debug_out"));
		debug_file_->Open();
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(debug_file_);
	}

	if(knob_->ValueBool("partial_instrument")) {
		//load static profile result
		if(knob_->ValueStr("static_profile").compare("0")!=0) {
			// char buffer[200];
			// std::fstream in(knob_->ValueStr("static_profile").c_str(),
			// 	std::ios::in);
			// while(!in.eof()) {
			// 	in.getline(buffer,200,'\n');
			// 	static_profile_.insert(std::string(buffer));
			// }
			// in.close();
			const char *filename=knob_->ValueStr("static_profile").c_str();
			LOAD_LINES_TO_SET(filename,static_profile_);
			//load the instrumented lines
			if(knob_->ValueStr("instrumented_lines").compare("0")!=0) {
				// in.open(knob_->ValueStr("instrumented_lines").c_str(),
				// 	std::ios::in);
				// while(!in.eof()) {
				// 	in.getline(buffer,100,'\n');
				// 	instrumented_lines_.insert(std::string(buffer));
				// }
				// in.close();
				const char *filename=knob_->ValueStr("instrumented_lines").c_str();
				LOAD_LINES_TO_SET(filename,instrumented_lines_);
			}		
		}
	}

	//Load static info.
	sinfo_=new StaticInfo(CreateMutex());
	sinfo_->Load(knob_->ValueStr("sinfo_in"));

	if(!sinfo_->FindImage(PSEUDO_IMAGE_NAME))
		sinfo_->CreateImage(PSEUDO_IMAGE_NAME);

	//add debug analyzer if necessary
	if(debug_analyzer_->Enabled()) {
		debug_analyzer_->Setup();
		AddAnalyzer(debug_analyzer_);
	}

	HandlePostSetup();

	//Setup call stack info if needed.
	if(desc_.TrackCallStack()) {
		callstack_info_=new CallStackInfo(CreateMutex());
		for(AnalyzerContainer::iterator it=analyzers_.begin();
			it!=analyzers_.end();it++) {
			Analyzer *analyzer=*it;
			//If analyzer needs track call stack
			if(analyzer->desc()->TrackCallStack())
				analyzer->setCallStackInfo(callstack_info_);
		}
		//Setup the call stack tracker.
		CallStackTracker *callstack_tracker
			=new CallStackTracker(callstack_info_);
		AddAnalyzer(callstack_tracker);
	}
}

bool ExecutionControl::FilterNonPotentialInstrument(std::string &filename,
	INT32 &line,INS ins)
{
	//if instrument the whole program
	if(!knob_->ValueBool("partial_instrument"))
		return false;

	if(filename.empty()) {
		PIN_GetSourceLocation(INS_Address(ins),NULL,&line,&filename);
		size_t found=filename.find_last_of("/");
		filename=filename.substr(found+1);
	}
	else
		PIN_GetSourceLocation(INS_Address(ins),NULL,&line,NULL);

	std::stringstream ss;
	ss<<filename<<" "<<line;

	if(instrumented_lines_.empty() || 
		instrumented_lines_.find(ss.str())==instrumented_lines_.end())
		return true;
	return false;
}

void ExecutionControl::InstrumentTrace(TRACE trace,VOID *v) 
{
	HandlePreInstrumentTrace(trace);

	std::string filename;
	INT32 line=0;

	//Do not need to hook memory and atomic inst
	if(!desc_.HookMem() && !desc_.HookAtomicInst()) {
		HandlePostInstrumentTrace(trace);
		return ;
	}	
	for(BBL bbl=TRACE_BblHead(trace);BBL_Valid(bbl);bbl=BBL_Next(bbl)) {
		//Get the corresponding img of this trace.
		IMG img=GetImgByTrace(trace);

		//Instrumentation to track the atomic inst.
		if(desc_.HookAtomicInst()) {
			for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {

				if(FilterNonPotentialInstrument(filename,line,ins))
					continue;

				//true if this instruction may do an atomic update of memory
				if(!INS_IsAtomicUpdate(ins))
					continue;

				Inst *inst=GetInst(INS_Address(ins));
				UpdateInstOpcode(inst,ins);

				INS_InsertCall(ins,IPOINT_BEFORE,
					(AFUNPTR)__BeforeAtomicInst,
					CALL_ORDER_BEFORE
					IARG_THREAD_ID,
					IARG_PTR,inst,
					IARG_UINT32,INS_Opcode(ins),
					IARG_MEMORYREAD_EA,
					IARG_END);
				//Insert a call on the fall through path of an instruction or 
				//return path of a routine. 
				if (INS_HasFallThrough(ins)) {
		          INS_InsertCall(ins, IPOINT_AFTER,
		                         (AFUNPTR)__AfterAtomicInst,
		                         CALL_ORDER_AFTER
		                         IARG_THREAD_ID,
		                         IARG_PTR, inst,
		                         IARG_UINT32, INS_Opcode(ins),
		                         IARG_END);
		        }
		        //Insert a call on the taken edge of branch
		        if (INS_IsBranchOrCall(ins)) {
		          INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
		                         (AFUNPTR)__AfterAtomicInst,
		                         CALL_ORDER_AFTER
		                         IARG_THREAD_ID,
		                         IARG_PTR, inst,
		                         IARG_UINT32, INS_Opcode(ins),
		                         IARG_END);
		        }
			}
		}// if(desc_.HookAtomicInst())

		//Decide whether to instrument mem access
		if(HandleIgnoreMemAccess(img))
			continue;

		//Instrumentation to track mem access
		if(desc_.HookMem()) {
			for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {

				if(FilterNonPotentialInstrument(filename,line,ins))
					continue;
//INFO_PRINT("==================potential instrument===================\n");
				//Only track memory access instructions.
				if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
					//Skip stack access if necessary
					if(desc_.SkipStackAccess())
						if(INS_IsStackRead(ins) || INS_IsStackWrite(ins))
							continue;

					Inst *inst=GetInst(INS_Address(ins));
					UpdateInstOpcode(inst,ins);

					//Instrument before mem accesses.
					if(desc_.HookBeforeMem()) {
						if(INS_IsMemoryRead(ins)) {
							INS_InsertCall(ins,IPOINT_BEFORE,
								(AFUNPTR)__BeforeMemRead,
								CALL_ORDER_BEFORE
								IARG_THREAD_ID,
								IARG_PTR,inst,
								IARG_MEMORYREAD_EA,
								IARG_MEMORYREAD_SIZE,
								IARG_END);
						}

						if(INS_IsMemoryWrite(ins)) {
							INS_InsertCall(ins,IPOINT_BEFORE,
								(AFUNPTR)__BeforeMemWrite,
								CALL_ORDER_BEFORE
								IARG_THREAD_ID,
								IARG_PTR,inst,
								IARG_MEMORYWRITE_EA,
								IARG_MEMORYWRITE_SIZE,
								IARG_END);
						}
						//true if this instruction has 2 memory read operands 
						if(INS_HasMemoryRead2(ins)) {
							INS_InsertCall(ins,IPOINT_BEFORE,
								(AFUNPTR)__BeforeMemRead2,
								CALL_ORDER_BEFORE
								IARG_THREAD_ID,
								IARG_PTR,inst,
								IARG_MEMORYREAD2_EA,
								IARG_MEMORYREAD_SIZE,
								IARG_END);
						}
					}
					// Instrument after mem accesses.
		          	if (desc_.HookAfterMem()) {
		            	if (INS_IsMemoryRead(ins)) {
		              		if (INS_HasFallThrough  (ins)) {
		                		INS_InsertCall(ins, IPOINT_AFTER,
		                               (AFUNPTR)__AfterMemRead,
		                               CALL_ORDER_AFTER
		                               IARG_THREAD_ID,
		                               IARG_PTR, inst,
		                               IARG_END);
		              		}

		             		if (INS_IsBranchOrCall(ins)) {
		                		INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
		                               (AFUNPTR)__AfterMemRead,
		                               CALL_ORDER_AFTER
		                               IARG_THREAD_ID,
		                               IARG_PTR, inst,
		                               IARG_END);
		              		}
		            	}

		            	if (INS_IsMemoryWrite(ins)) {
		              		if (INS_HasFallThrough(ins)) {
		                		INS_InsertCall(ins, IPOINT_AFTER,
		                             (AFUNPTR)__AfterMemWrite,
		                             CALL_ORDER_AFTER
		                             IARG_THREAD_ID,
		                             IARG_PTR, inst,
		                             IARG_END);
		              		}

		              		if (INS_IsBranchOrCall(ins)) {
		                		INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
		                             (AFUNPTR)__AfterMemWrite,
		                             CALL_ORDER_AFTER
		                             IARG_THREAD_ID,
		                             IARG_PTR, inst,
		                             IARG_END);
		              		}
		            	}

		            	if (INS_HasMemoryRead2(ins)) {
		              		if (INS_HasFallThrough(ins)) {
		                		INS_InsertCall(ins, IPOINT_AFTER,
		                               (AFUNPTR)__AfterMemRead2,
		                               CALL_ORDER_AFTER
		                               IARG_THREAD_ID,
		                               IARG_PTR, inst,
		                               IARG_END);
		              		}

		              		if (INS_IsBranchOrCall(ins)) {
		                		INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
		                               (AFUNPTR)__AfterMemRead2,
		                               CALL_ORDER_AFTER
		                               IARG_THREAD_ID,
		                               IARG_PTR, inst,
		                               IARG_END);
		              		}
		            	}
		          	} // if (desc_.HookAfterMem()) {
				}// if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
			}// for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {
		}// if(desc_.HookMem()) {

		//Instrumentation to track calls and returns.
		if(desc_.HookCallReturn()) {
			for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {
				if(INS_IsCall(ins)) {
					Inst *inst=GetInst(INS_Address(ins));
					UpdateInstOpcode(inst,ins);

					INS_InsertCall(ins,IPOINT_BEFORE,
						(AFUNPTR)__BeforeCall,
						CALL_ORDER_BEFORE
						IARG_THREAD_ID,
						IARG_PTR,inst,
						//Target address of this branch instruction
						IARG_BRANCH_TARGET_ADDR, 
						IARG_END);

					INS_InsertCall(ins,IPOINT_TAKEN_BRANCH,
						(AFUNPTR)__AfterCall,
						CALL_ORDER_AFTER
						IARG_THREAD_ID,
						IARG_PTR,inst,
						IARG_BRANCH_TARGET_ADDR,
						//Return address for function call, valid
						//only at the function entry point
						IARG_RETURN_IP,
						IARG_END);
				}// if(INS_IsCall(ins)) {

				if(INS_IsRet(ins)) {
					Inst *inst=GetInst(INS_Address(ins));
					UpdateInstOpcode(inst,ins);

					INS_InsertCall(ins,IPOINT_BEFORE,
						(AFUNPTR)__BeforeReturn,
						CALL_ORDER_BEFORE
						IARG_THREAD_ID,
						IARG_PTR,inst,
						IARG_BRANCH_TARGET_ADDR,
						IARG_END);

					INS_InsertCall(ins,IPOINT_TAKEN_BRANCH,
						(AFUNPTR)__AfterReturn,
						CALL_ORDER_AFTER
						IARG_THREAD_ID,
						IARG_PTR,inst,
						IARG_BRANCH_TARGET_ADDR,
						IARG_END);
				}// if(INS_IsRet(ins)) {
			} // for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {
		}//if(desc_.HookCallReturn()) {
	}//for(BBL bbl=TRACE_BblHead(trace);BBL_Valid(bbl);bbl=BBL_Next(bbl)) {

	HandlePostInstrumentTrace(trace);
}

//Use this to register a call back to catch the loading of an image 
void ExecutionControl::ImageLoad(IMG img,VOID *v)
{
	//replace functions using wrappers
	if(desc_.HookPthreadFunc())
		ReplacePthreadWrappers(img);
	else
		ReplacePthreadCreateWrapper(img);

	if(desc_.HookMallocFunc())
		ReplaceMallocWrappers(img);

	//instrument the start functions 
	//include main and thread start
	if(desc_.HookMainFunc())
		InstrumentStartupFunc(img);
	//if the image haven't been shown
	Image *image=sinfo_->FindImage(IMG_Name(img));
	if(!image)
		image=sinfo_->CreateImage(IMG_Name(img));

	HandleImageLoad(img,image);
}

void ExecutionControl::ImageUnload(IMG img,VOID *v)
{
	Image *image=sinfo_->FindImage(IMG_Name(img));
	DEBUG_ASSERT(image);
	HandleImageUnload(img,image);
}

void ExecutionControl::ContextChange(THREADID tid,CONTEXT_CHANGE_REASON reason,
	const CONTEXT *from ,CONTEXT *to,INT32 info,VOID *v) 
{
	// if(desc_.HookSignal()) {
	// 
	// }
}

void ExecutionControl::ProgramStart()
{
	HandleProgramStart();
}

void ExecutionControl::ProgramExit(INT32 code,VOID *v)
{
	HandleProgramExit();
	//save static info
	sinfo_->Save(knob_->ValueStr("sinfo_out"));
	//close debug file if exists
	if(debug_file_)
		debug_file_->Close();
	log_fini();
}

//Register a notification function that is called when a thread starts executing in the 
//application. The call-back happens even for the application's root (initial) thread.
void ExecutionControl::ThreadStart(THREADID tid,CONTEXT *ctxt,INT32 flags,VOID *v)
{
	thread_t curr_thd_id=PIN_ThreadUid();
	OS_THREAD_ID os_tid=PIN_GetTid();
	OS_THREAD_ID parent_os_tid=PIN_GetParentTid();

	LockKernel();
	tls_thd_clock_[tid]=0; //init thread clock
	thd_create_sem_map_[os_tid]=CreateSemaphore(0);
	os_tid_map_[os_tid]=curr_thd_id;

	//notify parent that the new thread start
	if(main_thread_started_) {
		DEBUG_ASSERT(parent_os_tid);
		//parent pthread_t ppid mapping to PIN THREAID
		child_thd_map_[parent_os_tid]=curr_thd_id;
		//notify the thread create function
		if(thd_create_sem_map_[parent_os_tid]->Post())
			Abort("NotifyNewChild:semphore post returns error\n");
	}
	UnlockKernel();
	HandleThreadStart();
	//if current thread is main thread
	if(!main_thread_started_) {
		main_thd_id_=curr_thd_id;
		main_thread_started_=true;
	}
}

void ExecutionControl::ThreadExit(THREADID tid,const CONTEXT *ctxt,INT32 code,VOID *v)
{
	HandleThreadExit();

	OS_THREAD_ID os_tid=PIN_GetTid();
	ScopedLock locker(kernel_lock_);
	//
	delete thd_create_sem_map_[os_tid];
	thd_create_sem_map_.erase(os_tid);
	os_tid_map_.erase(os_tid);
}


void ExecutionControl::HandlePreSetup() {
  	// empty (register knobs)
}

void ExecutionControl::HandlePostSetup() {
  	// setup analyzers
}

void ExecutionControl::HandlePreInstrumentTrace(TRACE trace) {
  	// extra instrumentation before regular instrumentation
}

void ExecutionControl::HandlePostInstrumentTrace(TRACE trace) {
  	// extra instrumentation after regular instrumentation
}

void ExecutionControl::HandleProgramStart()
{
	//each analyzer's program start
	CALL_ANALYSIS_FUNC(ProgramStart);
}

void ExecutionControl::HandleProgramExit()
{
	CALL_ANALYSIS_FUNC(ProgramExit);
}

//Following are handlers for uniinstrumented and instrumented functions
void ExecutionControl::HandleImageLoad(IMG img,Image *image)
{
	address_t low_addr=IMG_LowAddress(img);
	address_t high_addr=IMG_HighAddress(img);
	address_t data_start=0;
	size_t data_size=0;
	address_t bss_start=0;
	size_t bss_size=0;
	//elf section
	for(SEC sec=IMG_SecHead(img);SEC_Valid(sec);sec=SEC_Next(sec)) {
		if(SEC_Name(sec)==".data") {
			data_start=SEC_Address(sec);
			data_size=SEC_Size(sec);
		}
		if(SEC_Name(sec)==".bss") {
			bss_start=SEC_Address(sec);
			bss_size=SEC_Size(sec);
		}
	}
	//notify all analyzers which needs to do some image load handlings
	CALL_ANALYSIS_FUNC(ImageLoad,image,low_addr,high_addr,data_start,data_size,
		bss_start,bss_size);
}

void ExecutionControl::HandleImageUnload(IMG img, Image *image) 
{
  address_t low_addr = IMG_LowAddress(img);
  address_t high_addr = IMG_HighAddress(img);
  address_t data_start = 0;
  size_t data_size = 0;
  address_t bss_start = 0;
  size_t bss_size = 0;

  for(SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    if (SEC_Name(sec) == ".data") {
      data_start = SEC_Address(sec);
      data_size = SEC_Size(sec);
    }
    if (SEC_Name(sec) == ".bss") {
      bss_start = SEC_Address(sec);
      bss_size = SEC_Size(sec);
    }
  }

  CALL_ANALYSIS_FUNC(ImageUnload, image, low_addr, high_addr, data_start,
                     data_size, bss_start, bss_size);
}


void ExecutionControl::HandleThreadStart()
{
	thread_t self=Self();
	thread_t parent=GetParent();
	CALL_ANALYSIS_FUNC(ThreadStart,self,parent);
}

void ExecutionControl::HandleThreadExit()
{
	thread_t self=Self();
	timestamp_t curr_thd_clk=GetThdClk(PIN_ThreadId());
	CALL_ANALYSIS_FUNC(ThreadExit,self,curr_thd_clk);
}

void ExecutionControl::HandleMain(THREADID tid, CONTEXT *ctxt) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  //hook
  CALL_ANALYSIS_FUNC2(MainFunc, Main, self, curr_thd_clk);
}

void ExecutionControl::HandleThreadMain(THREADID tid, CONTEXT *ctxt) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(MainFunc, ThreadMain, self, curr_thd_clk);
}

void ExecutionControl::HandleBeforeMemRead(THREADID tid,Inst *inst,
	address_t addr,size_t size) 
{
	thread_t self=Self();
	timestamp_t curr_thd_clk=GetThdClk(tid);
	CALL_ANALYSIS_FUNC2(BeforeMem,BeforeMemRead,self,curr_thd_clk,inst,
		addr,size);
}


void ExecutionControl::HandleAfterMemRead(THREADID tid, Inst *inst,
                                          address_t addr, size_t size) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(AfterMem, AfterMemRead, self, curr_thd_clk,
                      inst, addr, size);
}

void ExecutionControl::HandleBeforeMemWrite(THREADID tid, Inst *inst,
                                            address_t addr, size_t size) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(BeforeMem, BeforeMemWrite, self, curr_thd_clk,
                      inst, addr, size);
}

void ExecutionControl::HandleAfterMemWrite(THREADID tid, Inst *inst,
                                           address_t addr, size_t size) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(AfterMem, AfterMemWrite, self, curr_thd_clk,
                      inst, addr, size);
}

void ExecutionControl::HandleBeforeAtomicInst(THREADID tid, Inst *inst,
                                              OPCODE opcode, address_t addr) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  std::string type = OPCODE_StringShort(opcode);
  CALL_ANALYSIS_FUNC2(AtomicInst, BeforeAtomicInst, self, curr_thd_clk,
                      inst, type, addr);
}

void ExecutionControl::HandleAfterAtomicInst(THREADID tid, Inst *inst,
                                             OPCODE opcode, address_t addr) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  std::string type = OPCODE_StringShort(opcode); //string with instruction opcode

  CALL_ANALYSIS_FUNC2(AtomicInst, AfterAtomicInst, self, curr_thd_clk,
                      inst, type, addr);
}


void ExecutionControl::HandleBeforeCall(THREADID tid, Inst *inst,
                                        address_t target) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(CallReturn, BeforeCall, self, curr_thd_clk,
                      inst, target);
}

void ExecutionControl::HandleAfterCall(THREADID tid, Inst *inst,
                                       address_t target, address_t ret) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(CallReturn, AfterCall, self, curr_thd_clk,
                      inst, target, ret);
}

void ExecutionControl::HandleBeforeReturn(THREADID tid, Inst *inst,
                                          address_t target) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(CallReturn, BeforeReturn, self, curr_thd_clk,
                      inst, target);
}

void ExecutionControl::HandleAfterReturn(THREADID tid, Inst *inst,
                                         address_t target) 
{
  thread_t self = Self();
  timestamp_t curr_thd_clk = GetThdClk(tid);
  CALL_ANALYSIS_FUNC2(CallReturn, AfterReturn, self, curr_thd_clk,
                      inst, target);
}

//Arround the wrapper handlers
void ExecutionControl::HandleBeforeWrapper(WrapperBase *wrapper) {
  // Nothing.
}

void ExecutionControl::HandleAfterWrapper(WrapperBase *wrapper) 
{
  // Simulate a return if call stack is being tracked. This is because the
  // return target will be changed by PIN (not transparent) for each function
  // that has a wrapper defined.
  if (desc_.TrackCallStack()) {
    DEBUG_ASSERT(callstack_info_);
    CallStack *callstack = callstack_info_->GetCallStack(Self());
    callstack->OnReturn(NULL, wrapper->ret_addr());
  }
}

void ExecutionControl::Abort(const std::string &msg) 
{
  fprintf(stderr, "%s", msg.c_str());
  assert(0);
}

Inst *ExecutionControl::GetInst(ADDRINT pc)
{
	Image *image=NULL;
	ADDRINT offset=0;

	PIN_LockClient();
	IMG img=IMG_FindByAddress(pc);
	//offset according the image
	//some instructions are not belong to neither executable nor librayies
	if(!IMG_Valid(img)) {
		image=sinfo_->FindImage(PSEUDO_IMAGE_NAME);
		offset=pc;
	}else {
		image=sinfo_->FindImage(IMG_Name(img));
		offset=pc-IMG_LowAddress(img);
	}
	DEBUG_ASSERT(image);
	Inst *inst=image->Find(offset);
	if(!inst) {
		inst=sinfo_->CreateInst(image,offset);
		UpdateInstDebugInfo(inst,pc);
	}
	PIN_UnlockClient();

	return inst;
}

void ExecutionControl::UpdateInstOpcode(Inst *inst,INS ins)
{
	//one instruction may have different opcode
	if(!inst->HasOpcode())
		inst->SetOpcode(INS_Opcode(ins));
}

void ExecutionControl::UpdateInstDebugInfo(Inst *inst,ADDRINT pc)
{
	if(!inst->HasDebugInfo()) {
		std::string file_name;
		int line=0;
		int column=0;
		//Also, acquiring the client lock is not required in
    	// instrumentation functions, only in analysis functions.
		PIN_GetSourceLocation(pc,&column,&line,&file_name);
		if(!file_name.empty())
			inst->SetDebugInfo(file_name,line,column);
	}
}

void ExecutionControl::AddAnalyzer(Analyzer *analyzer)
{
	analyzers_.push_back(analyzer);
	desc_.Merge(analyzer->desc());
}

thread_t ExecutionControl::GetThdId(pthread_t thread)
{
	ScopedLock lock(kernel_lock_);
	if(pthread_handle_map_.find(thread) == pthread_handle_map_.end())
		return main_thd_id_;
	else
		return pthread_handle_map_[thread];
}

thread_t ExecutionControl::GetParent()
{
	OS_THREAD_ID parent_os_tid=PIN_GetParentTid();
	ScopedLock lock(kernel_lock_);

	if(parent_os_tid)
		return os_tid_map_[parent_os_tid];
	else
		return INVALID_THD_ID;
}

//for wrappered pthread_create
thread_t ExecutionControl::WaitForNewChild(
	WRAPPER_CLASS(PthreadCreate) * wrapper)
{
	OS_THREAD_ID curr_os_tid=PIN_GetTid();
	pthread_t thread;
	size_t size;

	LockKernel();
	Semaphore *sem=thd_create_sem_map_[curr_os_tid];
	UnlockKernel();
	//wait the thread start ,so we can get the child thread's 
	//child_thd_id
	if(sem->Wait())
		Abort("WaitForNewChild:semaphore wait returns error\n");

	LockKernel();
	//get the lastest child thd id
	//here child_thd_map_ worked as a temporary transition
	thread_t child_thd_id=child_thd_map_[curr_os_tid];
	child_thd_map_.erase(curr_os_tid);
	//update pthread handle map

	size=PIN_SafeCopy(&thread,wrapper->arg0(),sizeof(pthread_t));
	assert(size==sizeof(pthread_t));
		
	//
	pthread_handle_map_[thread]=child_thd_id;
	UnlockKernel();

	return child_thd_id;
}


void ExecutionControl::ReplacePthreadCreateWrapper(IMG img)
{
	ACTIVATE_WRAPPER_HANDLER(PthreadCreate);
}

void ExecutionControl::ReplacePthreadWrappers(IMG img)
{
	ACTIVATE_WRAPPER_HANDLER(PthreadCreate);
  	ACTIVATE_WRAPPER_HANDLER(PthreadJoin);
  	ACTIVATE_WRAPPER_HANDLER(PthreadMutexTryLock);
  	ACTIVATE_WRAPPER_HANDLER(PthreadMutexLock);
  	ACTIVATE_WRAPPER_HANDLER(PthreadMutexUnlock);
  	ACTIVATE_WRAPPER_HANDLER(PthreadRwlockTryRdlock);
	ACTIVATE_WRAPPER_HANDLER(PthreadRwlockTryWrlock);
	ACTIVATE_WRAPPER_HANDLER(PthreadRwlockRdlock);
	ACTIVATE_WRAPPER_HANDLER(PthreadRwlockWrlock);
	ACTIVATE_WRAPPER_HANDLER(PthreadRwlockUnlock);
	ACTIVATE_WRAPPER_HANDLER(PthreadCondSignal);
	ACTIVATE_WRAPPER_HANDLER(PthreadCondBroadcast);
	ACTIVATE_WRAPPER_HANDLER(PthreadCondWait);
	ACTIVATE_WRAPPER_HANDLER(PthreadCondTimedwait);
	ACTIVATE_WRAPPER_HANDLER(PthreadBarrierInit);
	ACTIVATE_WRAPPER_HANDLER(PthreadBarrierWait);
	ACTIVATE_WRAPPER_HANDLER(SemInit);
	ACTIVATE_WRAPPER_HANDLER(SemPost);
	ACTIVATE_WRAPPER_HANDLER(SemWait);
}

void  ExecutionControl::ReplaceMallocWrappers(IMG img)
{
	ACTIVATE_WRAPPER_HANDLER(Malloc);
  	ACTIVATE_WRAPPER_HANDLER(Calloc);
  	ACTIVATE_WRAPPER_HANDLER(Realloc);
  	ACTIVATE_WRAPPER_HANDLER(Free);
}

//Used for image instrumentation
void ExecutionControl::InstrumentStartupFunc(IMG img)
{
	if(!IMG_IsMainExecutable(img) && 
		IMG_Name(img).find("libpthread")==std::string::npos)
		return ;

	for(SEC sec=IMG_SecHead(img);SEC_Valid(sec);sec=SEC_Next(sec)) {
		for(RTN rtn=SEC_RtnHead(sec);RTN_Valid(rtn);rtn=RTN_Next(rtn)) {
			//instrument main function
			if(IMG_IsMainExecutable(img) && 
				RTN_Name(rtn).compare("main")==0) {
				RTN_Open(rtn);
			RTN_InsertCall(rtn,IPOINT_BEFORE,
				(AFUNPTR)__Main,
				CALL_ORDER_BEFORE
				IARG_THREAD_ID,
				IARG_CONTEXT,
				IARG_END);
			RTN_Close(rtn);
			}

			//instrument thread startup functions
			if(IMG_Name(img).find("libpthread")!=std::string::npos &&
				RTN_Name(rtn).compare("start_thread")==0) {
				RTN_Open(rtn);
				RTN_InsertCall(rtn,IPOINT_BEFORE,
					(AFUNPTR)__ThreadMain,
					CALL_ORDER_BEFORE
					IARG_THREAD_ID,
					IARG_CONTEXT,
					IARG_END);

				RTN_Close(rtn);
			}
		}
	}
}

void ExecutionControl::__Main(THREADID tid,CONTEXT *ctxt)
{
	ctrl_->HandleMain(tid,ctxt);
}

void ExecutionControl::__ThreadMain(THREADID tid,CONTEXT *ctxt)
{
	ctrl_->HandleThreadMain(tid,ctxt);
}

void ExecutionControl::__BeforeMemRead(THREADID tid, Inst *inst,
                                       ADDRINT addr, UINT32 size) 
{
	ctrl_->HandleBeforeMemRead(tid,inst,addr,size);
	//Record the last read for after read memory handlers
	if(ctrl_->desc_.HookAfterMem()) {
		ctrl_->tls_read_addr_[tid]=addr;
		ctrl_->tls_read_size_[tid]=size;
	}
}

void ExecutionControl::__AfterMemRead(THREADID tid, Inst *inst)
{
	address_t addr=ctrl_->tls_read_addr_[tid];
	size_t size=ctrl_->tls_read_size_[tid];
	ctrl_->HandleAfterMemRead(tid,inst,addr,size);
}

void ExecutionControl::__BeforeMemWrite(THREADID tid, Inst *inst,
	ADDRINT addr, UINT32 size)
{
	ctrl_->HandleBeforeMemWrite(tid,inst,addr,size);
	if(ctrl_->desc_.HookAfterMem()) {
		ctrl_->tls_write_addr_[tid]=addr;
		ctrl_->tls_write_size_[tid]=size;
	}
}

void ExecutionControl::__AfterMemWrite(THREADID tid, Inst *inst) 
{
	address_t addr = ctrl_->tls_write_addr_[tid];
  	size_t size = ctrl_->tls_write_size_[tid];
  	ctrl_->HandleAfterMemWrite(tid, inst, addr, size);
}

void ExecutionControl::__BeforeMemRead2(THREADID tid, Inst *inst,
	ADDRINT addr, UINT32 size) 
{
	ctrl_->HandleBeforeMemRead(tid,inst,addr,size);
	if(ctrl_->desc_.HookAfterMem()) {
		ctrl_->tls_read2_addr_[tid]=addr;
		ctrl_->tls_read2_size_[tid]=size;
	}
}

void ExecutionControl::__AfterMemRead2(THREADID tid, Inst *inst) 
{
  	address_t addr = ctrl_->tls_read2_addr_[tid];
  	size_t size = ctrl_->tls_read_size_[tid];
  	ctrl_->HandleAfterMemRead(tid, inst, addr, size);
}

void ExecutionControl::__BeforeAtomicInst(THREADID tid, Inst *inst,
	UINT32 opcode, ADDRINT addr) 
{
	ctrl_->HandleBeforeAtomicInst(tid,inst,opcode,addr);
	ctrl_->tls_atomic_addr_[tid]=addr;

}

void ExecutionControl::__AfterAtomicInst(THREADID tid, Inst *inst,
	UINT32 opcode) 
{
	address_t addr = ctrl_->tls_atomic_addr_[tid];
  	ctrl_->HandleAfterAtomicInst(tid, inst, opcode, addr);
}


void ExecutionControl::__BeforeCall(THREADID tid, Inst *inst,
	ADDRINT target) 
{
  ctrl_->HandleBeforeCall(tid, inst, target);
}

void ExecutionControl::__AfterCall(THREADID tid, Inst *inst,
	ADDRINT target, ADDRINT ret) 
{
  ctrl_->HandleAfterCall(tid, inst, target, ret);
}

void ExecutionControl::__BeforeReturn(THREADID tid, Inst *inst,
	ADDRINT target) 
{
  ctrl_->HandleBeforeReturn(tid, inst, target);
}

void ExecutionControl::__AfterReturn(THREADID tid, Inst *inst,
	ADDRINT target) 
{
  ctrl_->HandleAfterReturn(tid, inst, target);
}


IMPLEMENT_WRAPPER_HANDLER(PthreadCreate,ExecutionControl)
{
	thread_t self=Self();
	Inst *inst=GetInst(wrapper->ret_addr());

	CALL_ANALYSIS_FUNC2(PthreadFunc,
		BeforePthreadCreate,
		self,
		GetThdClk(wrapper->tid()),
		inst);

	wrapper->CallOriginal();

	//wait until the new child thread start
	thread_t child_thd_id=WaitForNewChild(wrapper);

	CALL_ANALYSIS_FUNC2(PthreadFunc,
		AfterPthreadCreate,
		self,
		GetThdClk(wrapper->tid()),
		inst,
		child_thd_id);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadJoin,ExecutionControl)
{
	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	thread_t child = GetThdId(wrapper->arg0());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadJoin,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      child);

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadJoin,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      child);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexTryLock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexTryLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexTryLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexLock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexUnlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}
//
IMPLEMENT_WRAPPER_HANDLER(PthreadRwlockTryRdlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadRwlockTryRdlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadRwlockTryRdlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadRwlockTryWrlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadRwlockTryWrlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadRwlockTryWrlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadRwlockRdlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadRwlockRdlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadRwlockRdlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadRwlockWrlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadRwlockWrlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadRwlockWrlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadRwlockUnlock, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadRwlockUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadRwlockUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(Malloc, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeMalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterMalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      (address_t)wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(Calloc, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeCalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      wrapper->arg1());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterCalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      wrapper->arg1(),
                      (address_t)wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(Realloc, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeRealloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg1());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterRealloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg1(),
                      (address_t)wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(Free, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeFree,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterFree,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondSignal,ExecutionControl)
{
	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondSignal,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondSignal,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondBroadcast, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondBroadcast,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondBroadcast,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondWait, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondTimedwait, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondTimedwait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondTimedwait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadBarrierInit, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadBarrierInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadBarrierInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadBarrierWait, ExecutionControl) 
{
  	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadBarrierWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadBarrierWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(SemInit, ExecutionControl)
{
	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforeSemInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterSemInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());
}

IMPLEMENT_WRAPPER_HANDLER(SemPost, ExecutionControl)
{
	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforeSemPost,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterSemPost,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(SemWait, ExecutionControl)
{
	thread_t self = Self();
  	Inst *inst = GetInst(wrapper->ret_addr());

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforeSemWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  	wrapper->CallOriginal();

  	CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterSemWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

