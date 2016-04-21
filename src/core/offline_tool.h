/**
  * @file core/offline_tool.h
  * Define the abstract definition of offline tools.
  */

#ifndef __CORE_OFFLINE_TOOL_H
#define __CORE_OFFLINE_TOOL_H

#include "core/basictypes.h"
#include "core/log.h"
#include "core/sync.h"
#include "core/cmdline_knob.h"
#include "core/static_info.h"

class OfflineTool {
public:
	OfflineTool();
	virtual ~OfflineTool();

	void Initialize();
	void PreSetup();
	void PostSetup();
	void Parse(int argc,char *argv[]);
	void Start();
	void Exit();
protected:
	virtual Mutex *CreateMutex() { return new NullMutex; }
	virtual void HandlePreSetup();
	virtual void HandlePostSetup();
	virtual void HandleStart();
	virtual void HandleExit();

	Mutex *kernel_lock_;
	Knob *knob_;
	LogFile *debug_file_;
	StaticInfo *sinfo_;
	bool read_only_;

	static OfflineTool *tool_;
private:
	DISALLOW_COPY_CONSTRUCTORS(OfflineTool);
};

#endif // __CORE_OFFLINE_TOOL_H