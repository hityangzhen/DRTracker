#ifndef __PIN_KNOB_H
#define __PIN_KNOB_H

#include "pin.H"
#include "core/basictypes.h"
#include "core/knob.h"

#include <map>

class PinKnob:public Knob {
public:
	PinKnob(){}
	~PinKnob(){}

	void RegisterBool(const std::string &name,const std::string &desc,
		const std::string &val);
	void RegisterInt(const std::string &name,const std::string &desc,
		const std::string &val);
	void RegisterStr(const std::string &name,const std::string &desc,
		const std::string &val);

	bool ValueBool(const std::string &name);
	int ValueInt(const std::string &name);
	std::string ValueStr(const std::string &name);

	
	
private:
	typedef enum {
		KNOB_TYPE_INVALID=0,
		KNOB_TYPE_BOOL,
		KNOB_TYPE_INT,
		KNOB_TYPE_STR
	} KnobType;
	bool Exist(const std::string &name);
	typedef std::pair<KnobType,void *> TypedKnob;
	typedef std::map<std::string,TypedKnob> KnobNameMap;

	

	KnobNameMap knob_table_;
private:
	DISALLOW_COPY_CONSTRUCTORS(PinKnob);
};

#endif /* __PIN_KNOB_H */