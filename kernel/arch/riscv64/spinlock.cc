#include <types.hh>
#include <utils.hh>
#include <kern_logger/kern_logger.hh>

extern "C" {
    void acquire_lock_spin(u32 *lock) noexcept;
    void release_lock(u32 *lock) noexcept;
    bool try_lock(u32 *lock) noexcept;
}

void Spinlock::lock() noexcept
{
    acquire_lock_spin(&locked);
}

void Spinlock::unlock() noexcept
{
    release_lock(&locked);
}

bool Spinlock::try_lock() noexcept
{
    return ::try_lock(&locked);
}