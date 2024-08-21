#include <pthread.h>

int pthread_rwlock_rdlock(pthread_rwlock_t *lock)
{
    int res = pthread_mutex_lock(&lock->g);
    if (res != 0)
        return res;

    while (lock->num_writers_waiting || lock->writer_active) {
        res = pthread_cond_wait(&lock->cond, &lock->g);
        if (res != 0) {
            pthread_mutex_unlock(&lock->g);
            return res;
        }
    }

    lock->num_readers_active++;

    return pthread_mutex_unlock(&lock->g);
}