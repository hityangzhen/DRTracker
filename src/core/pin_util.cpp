#include "core/pin_util.h"
#include "core/log.h"

RTN FindRTN(IMG img,const std::string &funcName)
{
	RTN rtn=RTN_FindByName(img,funcName.c_str());
	if(RTN_Valid(rtn))
		return rtn;

	// handle those symbols with version numbers.
  	// e.g. pthread_create has global name: pthread_create@@GLIBC...
  	std::string funcNameV(funcName);
  	funcNameV.append("@@");
  	for(SYM sym=IMG_RegsymHead(img);SYM_Valid(sym);sym=SYM_Next(sym)) {
  		if(SYM_Name(sym).find(funcNameV)!=std::string::npos) {
  			RTN rtn=RTN_FindByAddress(SYM_Address(sym));
  			DEBUG_ASSERT(RTN_Valid(rtn));
  			return rtn;
  		}
  	}
  	return RTN_Invalid();
}

//Img=>Sec=>Rtn=>Trace
IMG GetImgByTrace(TRACE trace)
{
	IMG img=IMG_Invalid();
	RTN rtn=TRACE_Rtn(trace);
	if(RTN_Valid(rtn)) {
		SEC sec=RTN_Sec(rtn);
		if(SEC_Valid(sec))
			img=SEC_Img(sec);
	}
	return img;
}

bool BBLContainMemOp(BBL bbl)
{
	for(INS ins=BBL_InsHead(bbl);INS_Valid(ins);ins=INS_Next(ins)) {
		//exclude addresses  relative to esp,ebp
		if(INS_IsStackRead(ins) || INS_IsStackWrite(ins))
			continue;

		if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
			return true;
	}
	return false;
}