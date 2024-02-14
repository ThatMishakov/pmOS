#include "internal_rwlock.h"
#include "spin_pause.h"

void __internal_rwlock_read_lock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->r);
    lock->readers_count++;
    if (lock->readers_count == 1)
        __internal_rwlock_spinlock(&lock->g);
    __internal_rwlock_spinunlock(&lock->r);
}

void __internal_rwlock_read_unlock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->r);
    lock->readers_count--;
    if (lock->readers_count == 0)
        __internal_rwlock_spinunlock(&lock->g);
    __internal_rwlock_spinunlock(&lock->r);
}

void __internal_rwlock_write_lock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->g);
}

void __internal_rwlock_write_unlock(struct internal_rwlock *lock) {
    __internal_rwlock_spinunlock(&lock->g);
}



void __internal_rwlock_spinunlock(uint32_t *lock) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

void __internal_rwlock_spinlock(uint32_t *lock) {
    uint32_t expected = 0;
    while (1) {
        if (__atomic_compare_exchange_n(lock, &expected, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;

        while (__atomic_load_n(lock, __ATOMIC_RELAXED))
            spin_pause();
    }
}