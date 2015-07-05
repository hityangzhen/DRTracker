#ifndef __CORE_KNOB_H
#define __CORE_KNOB_H

#include "core/basictypes.h"

//Interface class for the command line switches.
class Knob {
public:
	Knob(){}
	virtual ~Knob(){}

	virtual void RegisterBool(const std::string &name,const std::string &desc,
		const std::string &val)=0;
	virtual void RegisterInt(const std::string &name,const std::string &desc,
		const std::string &val)=0;
	virtual void RegisterStr(const std::string &name,const std::string &desc,
		const std::string &val)=0;

	virtual bool ValueBool(const std::string &name)=0;
	virtual int ValueInt(const std::string &name)=0;
	virtual std::string ValueStr(const std::string &name)=0;
	
	static void Initialize(Knob *knob) { knob_=knob;}
	static Knob *Get() { return knob_; }
protected:
	static Knob *knob_;

private:
	DISALLOW_COPY_CONSTRUCTORS(Knob);
};



#endif /* __CORE_KNOB_H */