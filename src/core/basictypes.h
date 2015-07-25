#ifndef __CORE_BASICTYPES_H
#define __CORE_BASICTYPES_H

/**
 * Type definitions.
 */

#include <stdint.h>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

typedef signed char   schar;
typedef unsigned char uchar;
typedef int8_t        int8;
typedef int16_t       int16;
typedef int32_t       int32;
typedef int64_t       int64;
typedef uint8_t       uint8;
typedef uint16_t      uint16;
typedef uint32_t      uint32;
typedef uint64_t      uint64;

typedef uint64 thread_t;
typedef unsigned long address_t;  //gcc size 8 bytes
typedef uint64 timestamp_t; 

#define INVALID_THD_ID static_cast<thread_t>(-1)
#define INVALID_ADDRESS static_cast<address_t>(-1)
#define INVALID_THD_ID static_cast<thread_t>(-1)


//Used to align addresses
#define WORD_SIZE sizeof(void *)

#define UNIT_MASK(uint_size_) ((unit_size_)==0?(unit_size_):(~((unit_size_)-1)))

#define UNIT_DOWN_ALIGN(addr,unit_size_) ((unit_size_)==0?(addr):((addr) & UNIT_MASK(unit_size_))) 

#define UNIT_UP_ALIGN(addr,unit_size_) ((unit_size_)==0?(addr):(((addr)+(unit_size_)-1) & UNIT_MASK(unit_size_)))

//Disallow the copy constructor and operator= functions
#define DISALLOW_COPY_CONSTRUCTORS(Typename) 									\
	Typename(const Typename &);				 									\
	Typename & operator=(const Typename &)

#endif /* __CORE_BASICTYPES_H */
