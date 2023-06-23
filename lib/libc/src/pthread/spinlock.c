#include <pthread.h>
#include <errno.h>

int pthread_spin_trylock(pthread_spinlock_t* lock) {
    if (__sync_lock_test_and_set(lock, 1) == 0) {
        // Acquired the lock
        return 0;
    } else {
        // Failed to acquire the lock
        return EBUSY;
    }
}

int pthread_spin_lock(pthread_spinlock_t* lock) {
    while (__sync_lock_test_and_set(lock, 1) != 0) {
        // Spin until the lock is acquired
        while (*lock) {
            // Yield the CPU to allow other threads to run
            sched_yield();
        }
    }
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t* lock) {
    __sync_lock_release(lock);
    return 0;
}