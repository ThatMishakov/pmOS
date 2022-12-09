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

void t_print_bochs(const char *str, ...);

struct Spinlock {
	volatile bool locked = false;

	inline void lock()
	{
		// t_print_bochs("Debug: locking   %h\n", this);
		while (not __sync_bool_compare_and_swap(&locked, false, true));
		__sync_synchronize();
	}
	
	inline void unlock()
	{
		// t_print_bochs("Debug: unlocking %h\n", this);
		__sync_synchronize();
		locked = false;
	}

	bool operator==(const Spinlock& s) const
	{
		return this == &s;
	}
};

struct Auto_Lock_Scope {
	Spinlock& s;
	Auto_Lock_Scope(Spinlock& lock): s(lock)
	{
		s.lock();
	}

	~Auto_Lock_Scope()
	{
		s.unlock();
	}
};

struct Auto_Lock_Scope_Double {
	Spinlock& a;
	Spinlock& b;
	Auto_Lock_Scope_Double(Spinlock& a, Spinlock& b): a(&a < &b ? a : b), b(&a < &b ? b : a)
	{
		this->a.lock();
		if (a != b)
			this->b.lock();
	}

	~Auto_Lock_Scope_Double()
	{
		a.unlock();
		if (a != b)
			b.unlock();
	}
};