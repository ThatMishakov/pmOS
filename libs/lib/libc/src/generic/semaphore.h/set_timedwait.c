#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    if (pthread_mutex_lock(&sem->lock) != 0)
        return -1;
    
    while (sem->value <= 0) {
        if (pthread_cond_timedwait(&sem->cond, &sem->lock, abs_timeout) != 0) {
            pthread_mutex_unlock(&sem->lock);
            return -1;
        }
    }
    
    sem->value--;
    pthread_mutex_unlock(&sem->lock);
}