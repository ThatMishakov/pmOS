#pragma once
#include <kernel/types.h>

#define DECLARE_LOCK(name) volatile int name ## Locked
#define LOCK(name) \
	while (!__sync_bool_compare_and_swap(& name ## Locked, 0, 1)); \
	__sync_synchronize();
    
#define UNLOCK(name) \
	__sync_synchronize(); \
	name ## Locked = 0;

using kresult_t = u64;

template<class T>
struct ReturnStr {
    kresult_t result;
    T val;
};

struct Spinlock {
	volatile bool locked = false;

	inline void lock()
	{
		while (not __sync_bool_compare_and_swap(&locked, false, true));
		__sync_synchronize();
	}

	inline void unlock()
	{
		__sync_synchronize();
		locked = false;
	}
};