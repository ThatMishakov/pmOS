#include <kern_logger/kern_logger.hh>
#include <types.hh>
#include <utils.hh>

void acquire_lock_spin(u32 *lock) noexcept
{
    //size_t count = 0;
    while (true) {
        while (__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            // if (count++ > 1000000) {
            //     count = 0;
            //     serial_logger.printf("Warning: spinlock is busy\n");
            //     print_stack_trace(serial_logger);
            // }
        }

        if (!__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
            return;
    }
}
void release_lock(u32 *lock) noexcept { __atomic_store_n(lock, 0, __ATOMIC_RELEASE); }

constexpr int max_spins = 32;
bool try_lock(u32 *lock) noexcept { 
    for (int i = 0; i < max_spins; i++) {
        if (__atomic_load_n(lock, __ATOMIC_RELAXED))
            continue;

        if (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
            continue;

        return true;
    }
    return false;
}

void Spinlock_base::lock() noexcept { 
    acquire_lock_spin(&locked);
}

void Spinlock_base::unlock() noexcept { release_lock(&locked); }

bool Spinlock_base::try_lock() noexcept { return ::try_lock(&locked); }