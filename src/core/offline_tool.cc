#include "core/offline_tool.h"

OfflineTool *OfflineTool::tool_=NULL;

OfflineTool::OfflineTool():kernel_lock_(NULL),knob_(NULL),debug_file_(NULL),
	sinfo_(NULL),read_only_(false)
{}

OfflineTool::~OfflineTool()
{
	delete kernel_lock_;
	delete knob_;
	delete debug_file_;
	delete sinfo_;
}

void OfflineTool::Initialize()
{
	log_init(CreateMutex());
	kernel_lock_=CreateMutex();
	Knob::Initialize(new CmdlineKnob);
	knob_=Knob::Get();
	tool_=this;
}

void OfflineTool::PreSetup() {
  	knob_->RegisterStr("debug_out", "the output file for the debug messages", "stdout");
  	knob_->RegisterStr("sinfo_in", "the input static info database path", "sinfo.db");
  	knob_->RegisterStr("sinfo_out", "the output static info database path", "sinfo.db");

  	HandlePreSetup();
}

void OfflineTool::PostSetup()
{
	// setup debug output
	if(knob_->ValueStr("debug_out")=="stderr") {
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(stderrLogFile);
	} else if(knob_->ValueStr("debug_out")=="stdout") {
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(stdoutLogFile);
	} else {
		debug_file_=new FileLogFile(knob_->ValueStr("debug_out"));
		debug_file_->Open();
		debugLog->ResetLogFile();
		debugLog->RegisterLogFile(debug_file_);
	}
	// load static info
	sinfo_=new StaticInfo(CreateMutex());
	sinfo_->Load(knob_->ValueStr("sinfo_in"));

	HandlePostSetup();
}

void OfflineTool::Parse(int argc,char *argv[])
{
	((CmdlineKnob *)knob_)->Parse(argc,argv);
}

void OfflineTool::Start()
{
	HandleStart();
}

void OfflineTool::Exit()
{
	HandleExit();
	// save static info
	if(!read_only_)
		sinfo_in->Save(knob_->ValueStr("sinfo_out"));
	if(debug_file_)
		debug_file_->Close();
	// finilize log
	log_fini();
}

void OfflineTool::HandlePreSetup()
{}

void OfflineTool::HandlePostSetup()
{}

void OfflineTool::HandleStart()
{}

void OfflineTool::HandleExit()
{}