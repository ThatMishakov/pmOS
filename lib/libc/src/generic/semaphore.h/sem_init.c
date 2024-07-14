#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    if (pshared) {
        errno = ENOSYS;
        return -1;
    }

    if (value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }

    sem->value = value;
    
    int ret = pthread_mutex_init(&sem->lock, NULL);
    if (ret != 0) {
        return -1;
    }

    ret = pthread_cond_init(&sem->cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&sem->lock);
        return -1;
    }

    return 0;
}