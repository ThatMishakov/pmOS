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

inline int sched_yield(void)
{
    return 0;
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

int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
    if (lock == NULL) {
        return EINVAL;  // Invalid lock pointer
    }

    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL;  // Invalid pshared value
    }

    *lock = 0;  // Initialize the spinlock to an unlocked state

    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock) {
    if (lock == NULL) {
        return EINVAL;  // Invalid lock pointer
    }

    *lock = 0;  // Reset the lock variable

    return 0;
}