#ifndef __PIN_UTIL_H
#define __PIN_UTIL_H

#include "pin.H"
#include "core/basictypes.h"

#define CALL_ORDER_BEFORE
#define CALL_ORDER_AFTER

// Global definitions
//Find RTN by function name in an image.
extern RTN FindRTN(IMG img,const std::string &funcName);

//Get the IMG contains the TRACE.
extern IMG GetImgByTrace(TRACE trace);

//Return whethrer the given bbl contains no-stack memory access.
extern bool BBLContainMemOp(BBL bbl);

#endif /* __PIN_UTIL_H */

