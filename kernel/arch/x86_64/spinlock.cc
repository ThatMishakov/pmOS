#include "types.hh"
#include "utils.hh"
#include <kern_logger/kern_logger.hh>

int lock_wanted = true;
int lock_free = false;

void Spinlock::lock() noexcept
{
    int counter = 0;
    bool result;
    do {
        result = __atomic_compare_exchange(&locked, &lock_free, &lock_wanted, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
        if (counter++ == 10000000) {
            t_print_bochs("Possible deadlock at spinlock 0x%h... ", this);
            print_stack_trace(bochs_logger);
        }
    } while (not result);
}

void Spinlock::unlock() noexcept
{
	__atomic_store_n(&locked, false, __ATOMIC_RELEASE);
}

bool Spinlock::try_lock() noexcept
{
    return __atomic_compare_exchange(&locked, &lock_free, &lock_wanted, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

extern "C" void acquire_lock_spin(int *var)
{
    int counter = 0;
    bool result;
    do {
        result = __atomic_compare_exchange(var, &lock_free, &lock_wanted, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
        if (counter++ == 10000000) {
            t_print_bochs("Possible deadlock at spinlock 0x%h... ", var);
            print_stack_trace(bochs_logger);
        }
    } while (not result);
}

extern "C" void release_lock(int *var)
{
    __atomic_store_n(var, false, __ATOMIC_RELEASE);
}