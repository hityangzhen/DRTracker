#ifndef __CORE_LOG_H
#define __CORE_LOG_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>

#include "core/basictypes.h"
#include "core/sync.h"
//Log file interface
class LogFile {
public:
	LogFile(const std::string &nameIn):name(nameIn) {}
	virtual ~LogFile() {}

	virtual void Open()=0;
	virtual void Close()=0;
	virtual void Write(const std::string &msg)=0;
	virtual void Flush()=0;
	virtual bool IsOpen()=0;
protected:
	std::string name;
private:
	DISALLOW_COPY_CONSTRUCTORS(LogFile);
};

//File log file.The log will be written to a file.
class FileLogFile:public LogFile {
public:
	FileLogFile(const std::string &name):LogFile(name) {}
	~FileLogFile() {}

	void Open() { out.open(name.c_str()); }
	void Close() {out.close();}
	void Write(const std::string &msg) {out<<msg;}
	void Flush() {out.flush();}
	bool IsOpen() {return out.is_open();}

private:
	std::ofstream out;
	DISALLOW_COPY_CONSTRUCTORS(FileLogFile);
};

//Standard log file.The log file will be written to stdout/stderr.
class StdLogFile:public LogFile {
public:
	StdLogFile(const std::string &name):LogFile(name),out(NULL) {}
	~StdLogFile() {}

	void Open();
	void Close() {};
	void Write(const std::string &msg) {*out<<msg;}
	void Flush(){out->flush();}
	bool IsOpen(){return (out?out->good():false);}
private:
	std::ostream *out;
	DISALLOW_COPY_CONSTRUCTORS(StdLogFile);
};
//Define a log.A log can have multiple log files sa that it can write
//to multiple output ports.
class LogType {
public:
	LogType(bool enable,bool terminate,bool buffered,
		const std::string &prefix);
	~LogType() {}

	void ResetLogFile() {logFiles.clear();}
	void RegisterLogFile(LogFile *logFile) {logFiles.push_back(logFile);}
	void Message(const std::string &msg,bool printPrefix=true);
	bool On(){return enable;}
	void Enable() {enable=true;}
	void Disable(){enable=false;}
	void CloseLogFiles();
private:
	std::vector<LogFile *> logFiles;
	bool enable;
	bool terminate;
	bool buffered;
	std::string prefix;

	DISALLOW_COPY_CONSTRUCTORS(LogType);
};

//Global definitions
extern LogFile *stdoutLogFile;
extern LogFile *stderrLogFile;
extern LogType *assertLog;
extern LogType *debugLog;
extern LogType *infoLog;
extern Mutex *printLock;

extern void log_init(Mutex *lock);
extern void log_fini();

#define SEPARATOR "===================="

#define LOG_MSG(log,msg) do { \
	if((log)->On()) \
		(log)->Message(msg); \
} while(0)

//c++ 99
#define LOG_FMT_MSG(log,fmt,...) do { \
	char buffer[512]; \
	snprintf(buffer,512,(fmt),## __VA_ARGS__); \
	if((log)->On()) \
		(log)->Message(std::string(buffer)); \
}while(0)

//Define assert print utility
#define __ASSERT(msg) LOG_MSG(assertLog,msg)
#define SANITY_ASSERT(cond) do { \
	if(!(cond)) { \
		std::stringstream ss; \
		ss<<__FILE__<<": "<<__FUNCTION__<<": "<<__LINE__<<std::endl; \
		__ASSERT(ss.str()); \
	} \
}while(0)

#ifdef DEBUG
#define DEBUG_ASSERT(cond) SANITY_ASSERT(cond)
#else
#define DEBUG_ASSERT(cond) do {}while(0)
#endif /* DEBUG */


//Define info print utility
#define __INFO(msg) LOG_MSG(infoLog,msg)
#define __INFO_FMT(fmt,...) LOG_FMT_MSG(infoLog,fmt,##__VA_ARGS__)

#define INFO_PRINT(msg) __INFO(msg)
#define INFO_FMT_PRINT(fmt,...) __INFO_FMT(fmt,##__VA_ARGS__)
#define INFO_FMT_PRINT_SAFE(fmt,...) do { \
	printLock->Lock(); \
 	__INFO_FMT(fmt,##__VA_ARGS__); \
 	printLock->Unlock() ; \
}while(0)


//Define debug print utility
#define __DEBUG(msg) LOG_MSG(debugLog,msg)
#define __DEBUG_FMT(fmt,...) LOG_FMT_MSG(debugLog,fmt,##__VA_ARGS__)
#ifdef DEBUG
#define DEBUG_PRINT(msg) __DEBUG(msg)
#define DEBUG_FMT_PRINT(fmt,...) __DEBUG_FMT(fmt,##__VA_ARGS__)
#define DEBUG_FMT_PRINT_SAFE(fmt,...) do { \
	printLock->Lock(); \
	__DEBUG_FMT(fmt,##__VA_ARGS__); \
	printLock->Unlock(); \
}while(0)
#else
#define DEBUG_PRINT(msg) do {}while(0)
#define DEBUG_FMT_PRINT(fmt,...) do {}while(0)
#define DEBUG_FMT_PRINT_SAFE(fmt,...) do {}while(0)
#endif /* DEBUG */


#endif /* __CORE_LOG_H */