#pragma once
#include <stdint.h>
#include "common/types.h"

#define DECLARE_LOCK(name) volatile int name ## Locked
#define LOCK(name) \
	while (!__sync_bool_compare_and_swap(& name ## Locked, 0, 1)); \
	__sync_synchronize();
    
#define UNLOCK(name) \
	__sync_synchronize(); \
	name ## Locked = 0;

using kresult_t = uint64_t;

template<class T>
struct ReturnStr {
    kresult_t result;
    T val;
};