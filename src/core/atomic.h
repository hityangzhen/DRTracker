#ifndef __CORE_ATOMIC_H
#define __CORE_ATOMIC_H

/**
 * GCC atomic operations
 */

//These builtins perform the operation suggested by the name, 
//and returns the value that had previously been in memory

#define ATOMIC_FETCH_AND_ADD(ptr,value) \
    __sync_fetch_and_add((ptr), (value))
#define ATOMIC_FETCH_AND_SUB(ptr,value) \
    __sync_fetch_and_sub((ptr), (value))
#define ATOMIC_FETCH_AND_OR(ptr,value) \
    __sync_fetch_and_or((ptr), (value))
#define ATOMIC_FETCH_AND_AND(ptr,value) \
    __sync_fetch_and_and((ptr), (value))
#define ATOMIC_FETCH_AND_XOR(ptr,value) \
    __sync_fetch_and_xor((ptr), (value))
#define ATOMIC_FETCH_AND_NAND(ptr,value) \
    __sync_fetch_and_nand((ptr), (value))


// These builtins perform the operation suggested by the name, and
// return the new value.
#define ATOMIC_ADD_AND_FETCH(ptr,value) \
    __sync_add_and_fetch((ptr), (value))
#define ATOMIC_SUB_AND_FETCH(ptr,value) \
    __sync_sub_and_fetch((ptr), (value))
#define ATOMIC_OR_AND_FETCH(ptr,value) \
    __sync_or_and_fetch((ptr), (value))
#define ATOMIC_AND_AND_FETCH(ptr,value) \
    __sync_and_and_fetch((ptr), (value))
#define ATOMIC_XOR_AND_FETCH(ptr,value) \
    __sync_xor_and_fetch((ptr), (value))
#define ATOMIC_NAND_AND_FETCH(ptr,value) \
    __sync_nand_and_fetch((ptr), (value))

// These builtins perform an atomic compare and swap. That is, if the
// current value of *ptr is oldval, then write newval into *ptr.
// The "bool" version returns true if the comparison is successful
// and newval was written. The "val" version returns the contents of
// *ptr before the operation.
#define ATOMIC_BOOL_COMPARE_AND_SWAP(ptr,oldval,newval) \
    __sync_bool_compare_and_swap((ptr), (oldval), (newval))
#define ATOMIC_VAL_COMPARE_AND_SWAP(ptr,oldval,newval) \
    __sync_val_compare_and_swap((ptr), (oldval), (newval))



#endif