#include <pthread.h>

int pthread_rwlock_wrlock(pthread_rwlock_t *lock)
{
    int res = pthread_mutex_lock(&lock->g);
    if (res != 0)
        return res;

    lock->num_writers_waiting++;

    while (lock->num_readers_active || lock->writer_active) {
        res = pthread_cond_wait(&lock->cond, &lock->g);
        if (res != 0) {
            pthread_mutex_unlock(&lock->g);
            return res;
        }
    }

    lock->num_writers_waiting--;
    lock->writer_active = 1;

    return pthread_mutex_unlock(&lock->g);
}