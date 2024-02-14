#ifndef INTERNAL_RWLOCK_H
#define INTERNAL_RWLOCK_H

// An internal implementation of a read-write lock. Although this provides
// the same functionality as the pthread_rwlock_t type (and worse performance),
// it is using spinlocks which don't make syscalls and thus can never fail, which
// is wanted for stuff like pthread_key_create which must call destructors
// on thread exit, needing to lock key structure, where failure cannot be handled.
//
// I believe this is a good tradeoff, since I think it doesn't make much difference
// how quickly threads can terminate, and it's better to have something simple with
// a guarantee that destructors could be called even when everything is on fire.

#include <stdint.h>
#include <stdbool.h>

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_two_mutexes
struct internal_rwlock {
    uint32_t r;
    uint32_t g;
    uint64_t readers_count;
};

#define __INTERNAL_RWLOCK_INITIALIZER { 0, 0, 0 }

void __internal_rwlock_spinlock(uint32_t *lock);
void __internal_rwlock_spinunlock(uint32_t *lock);

void __internal_rwlock_read_lock(struct internal_rwlock *lock);
void __internal_rwlock_read_unlock(struct internal_rwlock *lock);
void __internal_rwlock_write_lock(struct internal_rwlock *lock);
void __internal_rwlock_write_unlock(struct internal_rwlock *lock);

#endif // INTERNAL_RWLOCK_H