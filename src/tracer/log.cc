#include "trace/log.h"
#include <cassert>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "core/log.h"

namespace tracer
{

#define LOG_SLICE_SIZE (1024 *128)

TraceLog::TraceLog(const std::string &path):path_(path),mode_(OP_MODE_INVALID),
	meta_(NULL),curr_slice_(NULL),entry_cursor_(0),has_next_(false)
{}

void TraceLog::OpenForRead()
{
	//set the mode
	mode_=OP_MODE_READ;
	//prepare dir
	PrepareDirForRead();
	//read meta
	std::stringstream meta_ss;
	meta_ss<<path_<<"/meta";
	std::fstream meta_in;
	meta_in.open(meta_ss.str().c_str(),std::ios::in | std::ios::binary);
	DEBUG_ASSERT(meta_in.is_open());
	meta_=new LogMetaProto;
	//deserialize
	meta->ParseFromIstream(&meta_in);
	meta_in.close();
	//read the first slice
	std::stringstream slice_ss;
	slice_ss<<path_<<"/1";
	std::fstream slice_in;
	slice_in.open(slice_in.str().c_str(),std::ios::in | std::ios::binary);
	DEBUG_ASSERT(slice_in.is_open());
	curr_slice_=new LogSliceProto;
	curr_slice_->ParseFromIstream(&slice_in);
	slice_in.close();

	DEBUG_ASSERT(meta->uid()==curr_slice_->uid());
	//set the cursor
	entry_cursor_=0;
	if(curr_slice_->entry_size())
		has_next_=true;
}

void TraceLog::OpenForWrite()
{
	//set the mode
	mode_=OP_MODE_WRITE;
	//clear and create path
	PrepareDirForWrite();
	//create meta
	trace_log_t uid=GenUid();
	meta_=new LogMetaProto;
	meta_->set_uid(uid);
	meta_->set_slice_count(1);
	//create the current slice
	curr_slice_=new LogSliceProto;
	curr_slice_->set_uid(uid);
	curr_slice_->set_slice_no(meta_->slice_count());	
}

void TraceLog::CloseForRead()
{
	//reclaim resource
	curr_slice_->Clear();
	meta_->Clear();
}

void TraceLog::CloseForWrite()
{
	//write current slice
	std::stringstream slice_ss;
	slice_ss<<path_<<"/"<<std::dec<<curr_slice_->slice_no();
	std::fstream slice_out;
	slice_out.open(slice_ss.str().c_str(),std::ios::out | std::ios::trunc |
		std::ios::binary);
	assert(slice_out.is_open());
	curr_slice_->SerializeToOstream(&slice_out);
	slice_out.close();
	//write meta
	std::stringstream meta_ss;
	meta_ss<<path_<<"/meta";
	std::fstream meta_out;
	meta_out.open(meta_ss.str().c_str(),std::ios::out | std::ios::trunc |
		std::ios::binary);
	assert(meta_out.is_open());
	meta_->SerializeToOstream(&meta_out);
	meta_out.close();
	//reclaim resource
	curr_slice_->Clear();
	meta_->Clear();
}

bool TraceLog::HasNextEntry()
{
	DEBUG_ASSERT(mode_==OP_MODE_READ);
	if(!has_next_)
		SwitchSliceForRead();
	return has_next_;
}

LogEntry TraceLog::NextEntry()
{
	DEBUG_ASSERT(mode_==OP_MODE_READ);
	DEBUG_ASSERT(has_next_);
	DEBUG_ASSERT(entry_cursor_>=0 && entry_cursor_<curr_slice_->entry_size());
	LogEntryProto *entry_proto=curr_slice_->mutable_entry(entry_cursor_++);
	if(entry_cursor_==curr_slice_->entry_size())
		has_next_=false;
	return LogEntry(entry_proto);
}

LogEntry TraceLog::NewEntry()
{
	DEBUG_ASSERT(mode_==OP_MODE_WRITE);
	if(curr_slice_->entry_size()>=LOG_SLICE_SIZE)
		SwitchSliceForWrite();
	//allocate the memory for the entry
	LogEntryProto *entry_proto=curr_slice_->add_entry();
	return LogEntry(entry_proto);
}

trace_log_t TraceLog::GenUid()
{
	return (trace_log_t)time(NULL);
}

void TraceLog::SwitchSliceForRead()
{
	DEBUG_ASSERT(mode_==OP_MODE_READ);
	uint32 curr_slice_no=curr_slice_->slice_no();
	uint32 next_slice_no=curr_slice_no+1;
	curr_slice_->Clear();
	//check whether the slice exists
	std::stringstream slice_ss;
	slice_ss<<path_<<"/"<<std::dec<<next_slice_no;
	std::fstream slice_in;
	slice_in.open(slice_ss.str().c_str(),std::ios::in | std::ios::binary);
	if(slice_in.is_open()) {
		curr_slice_->ParseFromIstream(&slice_in);
		slice_in.close();
		DEBUG_ASSERT(curr_slice_->entry_size());
		entry_cursor_=0;
		has_next_=true;
	} else
		has_next_=false;
}

void TraceLog::SwitchSliceForWrite()
{
	DEBUG_ASSERT(mode_==OP_MODE_WRITE);
	uint32 curr_slice_no=curr_slice_->slice_no();
	uint32 next_slice_no=curr_slice_no+1;
	//save the current slice
	std::stringstream slice_ss;
	slice_ss<<path_<<"/"<<std::dec<<curr_slice_no;
	std::fstream slice_out;
	slice_out.open(slice_ss.str().c_str(),std::ios::out | std::ios::trunc |
		std::ios::binary);
	assert(slice_out.is_open());
	curr_slice_->SerializeToOstream(&slice_out);
	slice_out.close();
	curr_slice_->Clear();
	//create the new slice
	curr_slice_->set_uid(meta_->uid());
	curr_slice_->set_slice_no(next_slice_no);
	meta_->set_slice_count(next_slice_no);
}

void TraceLog::PrepareDirForRead()
{
	DEBUG_ASSERT(mode_==OP_MODE_READ);
	struct stat sb;
	//stat store the file info into the struct stat
	if(stat(path_.c_str(),&sb) || !S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "please check the trace log dir.\n");
		assert(0);
	}
}

void TraceLog::PrepareDirForWrite()
{
	DEBUG_ASSERT(mode_==OP_MODE_WRITE);
	std::string cmd;
	int res;
	cmd.append("rm -rf ");
	cmd.append(path_);
	res=system(cmd.c_str());
	assert(!res);
	res=mkdir(path_.c_str(),0755);
	assert(!res);
}

} //namespace tracer