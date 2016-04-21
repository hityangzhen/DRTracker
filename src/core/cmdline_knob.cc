#include "core/cmdline_knob.h"
#include "core/log.h"

CmdlineKnob::CmdlineKnob()
{}

CmdlineKnob::~CmdlineKnob()
{
	for(KnobNameMap::iterator iter=knob_table_.begin();
		iter!=knob_table_.end();iter++) {
		delete iter->second.second;
	}
}

inline bool CmdlineKnob::Exist(const std::string &name)
{
	return knob_table_.find(name)!=knob_table_.end();	
}

/*
struct option {  
    // name of long option   
    const char *name;  
     
    // one of no_argument, required_argument, and optional_argument: 
    // whether option takes an argument 
    int has_arg;  

    // if not NULL, set *flag to val when option found   
    int *flag;  
    
    // if flag not NULL, value to set *flag to; else return value   
    int val;  
}; 
*/
void CmdlineKnob::Parse(int argc,char *argv[])
{
	// create long options and optstring
	struct option *long_options=new struct option[knob_table_.size()+1];
	std::string optstring;
	std::map<int,TypedKnob *> opt_map;
	int idx=0;

	for(KnobNameMap::iterator iter=knob_table_.begin();
		iter!=knob_table_.end();++iter,++idx) {
		long_options[idx].name=iter->first.c_str();
		long_options[idx].has_arg=optional_argument;
		long_options[idx].flag=NULL;
		// short option name-char
		long_options[idx].val='a'+idx;
		// concate the shot options 
		optstring.append(1,(char)long_options[idx].val);
		optstring.append(":");
		opt_map[long_options[idx].value]=&iter->second;
	}
	long_options[idx].name=NULL;
	long_options[idx].has_arg=0;
	long_options[idx].flag=NULL;
	long_options[idx].val=0;

	// start parsing
	/*
		int getopt_long(int argc, char * const argv[],const char *optstring,
	 		const struct option *longopts, int *longindex);
	 	return the short option if one option is parsed
	 */

	while(1) {
		int option_index=0;
		int c=getopt_long(argc,argv,optstring.c_str(),long_options,
			&option_index);
		if(c==-1)
			break;
		TypedKnob *knob=opt_map[c];
		DEBUG_ASSERT(knob);

		if(knob->first==KNOB_TYPE_BOOL)
			*((bool *)knob->second)=atoi(optarg)?true:false;
		else if (knob->first==KNOB_TYPE_INT)
			*((int *)knob->second)=atoi(optarg);
		else if(knob->first==KNOB_TYPE_STR)
			*((std::string *)knob->second)=std::string(optarg);
		else
			DEBUG_ASSERT(0);
	}

	delete []long_options;
}

void CmdlineKnob::RegisterBool(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name))
		return ;
	bool *value=new bool;
	if(atoi(val.c_str()))
		*value=true;
	else
		*value=false;
	knob_table_[name]=TypedKnob(KNOB_TYPE_BOOL,value);
}

void CmdlineKnob::RegisterInt(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name))
		return ;
	int *value=new int;
	*value=atoi(val.c_str());
	knob_table_[name]=TypedKnob(KNOB_TYPE_INT,value);
}

void CmdlineKnob::RegisterStr(const std::string &name,const std::string &desc,
	const std::string &val)
{
	if(Exist(name))
		return ;
	std::string *value=new std::string(value);
	knob_table_[name]=TypedKnob(KNOB_TYPE_STR,value);
}

bool CmdlineKnob::ValueBool(const std::string &name)
{
	KnobNameMap::iterator iter=knob_table_.find(name);
	DEBUG_ASSERT(iter!=knob_table_.end() && iter->second.first==KNOB_TYPE_BOOL);
	return *((bool *)iter->second.second);
}

int CmdlineKnob::ValueInt(const std::string &name)
{
	KnobNameMap::iterator iter=knob_table_.find(name);
	DEBUG_ASSERT(iter!=knob_table_.end() && iter->second.first==KNOB_TYPE_INT);
	return *((int *)iter->second.second);
}

std::string CmdlineKnob::ValueStr(const std::string &name)
{
	KnobNameMap::iterator iter=knob_table_.find(name);
	DEBUG_ASSERT(iter!=knob_table_.end() && iter->second.first==KNOB_TYPE_STR);
	return *((bool *)iter->second.second);
}