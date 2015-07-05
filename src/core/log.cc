#include "core/log.h"

void StdLogFile::Open() 
{
	if(name.compare("stdout")==0)
		out=&std::cout;
	else if(name.compare("stderr")==0)
		out=&std::cerr;
}

LogType::LogType(bool enableIn,bool terminateIn,bool bufferedIn,
	const std::string &prefixIn)
	:enable(enableIn),
	terminate(terminateIn),
	buffered(bufferedIn),
	prefix(prefixIn)
	{}

//write messages to every logfile
void LogType::Message(const std::string &msg,bool printPrefix)
{
	if(!enable) return ;

	for(std::vector<LogFile *>::iterator it=logFiles.begin();
		it!=logFiles.end();it++) {
		LogFile *logFile=*it;
		if(logFile->IsOpen()) {
			if(printPrefix)
				logFile->Write(prefix);
			logFile->Write(msg);
			if(!buffered)
				logFile->Flush();
		}
	}
	
	if(terminate) {
		for(std::vector<LogFile *>::iterator it=logFiles.begin();
			it!=logFiles.end();it++) {
			LogFile *logFile=*it;
			if(logFile->IsOpen())
				logFile->Flush();
		}
		abort();	
	}
}

void LogType::CloseLogFiles()
{
	for(std::vector<LogFile *>::iterator it=logFiles.begin();
		it!=logFiles.end();it++) {
		(*it)->Close();
	}
}

//Standard output/error stream
LogFile *stdoutLogFile=NULL;
LogFile *stderrLogFile=NULL;

LogType *assertLog=NULL;
LogType *debugLog=NULL;
LogType *infoLog=NULL;

//Print lock
Mutex *printLock=NULL;

//Global functions
void log_init(Mutex *lock)
{
	printLock=lock;

	//create default log files
	stdoutLogFile=new StdLogFile("stdout");
	stderrLogFile=new StdLogFile("stderr");
	
	stdoutLogFile->Open();
	stderrLogFile->Open();

	//create default log types
	assertLog=new LogType(true,true,false,"[ASSERT]");
	debugLog=new LogType(true,false,false,"[DEBUG]");
	infoLog=new LogType(true,false,false,"[INFO]");

	//register default log files for default log types
	assertLog->RegisterLogFile(stderrLogFile);
	debugLog->RegisterLogFile(stderrLogFile);
	infoLog->RegisterLogFile(stderrLogFile);
}

void log_fini()
{
	assertLog->Disable();
	debugLog->Disable();
	infoLog->Disable();
	//do nothing
	stdoutLogFile->Close();
	stderrLogFile->Close();
}

