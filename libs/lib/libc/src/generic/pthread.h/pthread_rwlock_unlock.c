#include <pthread.h>

int pthread_rwlock_unlock(pthread_rwlock_t *lock)
{
    int res = pthread_mutex_lock(&lock->g);
    if (res != 0)
        return res;

    if (lock->writer_active) {
        lock->writer_active = 0;
    
        if (lock->num_writers_waiting) {
            res = pthread_cond_signal(&lock->cond);
            if (res != 0) {
                pthread_mutex_unlock(&lock->g);
                return res;
            }
        } else {
            res = pthread_cond_broadcast(&lock->cond);
            if (res != 0) {
                pthread_mutex_unlock(&lock->g);
                return res;
            }
        }
    } else {
        lock->num_readers_active--;
        if (lock->num_readers_active == 0 && lock->num_writers_waiting) {
            res = pthread_cond_signal(&lock->cond);
            if (res != 0) {
                pthread_mutex_unlock(&lock->g);
                return res;
            }
        }
    }

    return pthread_mutex_unlock(&lock->g);
}