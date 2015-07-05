#include "core/pin_knob.h"
#include "core/log.h"

bool PinKnob::Exist(const std::string &name)
{
	KnobNameMap::iterator it=knob_table_.find(name);
	if(it==knob_table_.end())
		return false;
	return true;
}

void PinKnob::RegisterBool(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name)) return ;

	KNOB<bool> *knob=new KNOB<bool>(KNOB_MODE_WRITEONCE,"pintool",
		name,val,desc);
	knob_table_[name]=TypedKnob(KNOB_TYPE_BOOL,knob);
}

void PinKnob::RegisterInt(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name)) return ;

	KNOB<int> *knob=new KNOB<int>(KNOB_MODE_WRITEONCE,"pintool",
		name,val,desc);
	knob_table_[name]=TypedKnob(KNOB_TYPE_INT,knob);
}

void PinKnob::RegisterStr(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name)) return ;

	KNOB<std::string> *knob=new KNOB<std::string>(KNOB_MODE_WRITEONCE,"pintool",
		name,val,desc);
	knob_table_[name]=TypedKnob(KNOB_TYPE_STR,knob);
}

bool PinKnob::ValueBool(const std::string &name)
{
	KnobNameMap::iterator it=knob_table_.find(name);
	DEBUG_ASSERT(it!=knob_table_.end() && it->second.first==KNOB_TYPE_BOOL);
	return ((KNOB<bool> *)it->second.second)->Value();
}

int PinKnob::ValueInt(const std::string &name)
{
	KnobNameMap::iterator it=knob_table_.find(name);
	DEBUG_ASSERT(it!=knob_table_.end() && it->second.first==KNOB_TYPE_INT);
	return ((KNOB<int> *)it->second.second)->Value();
}

std::string PinKnob::ValueStr(const std::string &name)
{
	KnobNameMap::iterator it=knob_table_.find(name);
	DEBUG_ASSERT(it!=knob_table_.end() && it->second.first==KNOB_TYPE_STR);
	return ((KNOB<std::string> *)it->second.second)->Value();
}