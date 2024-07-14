#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

int sem_wait(sem_t *sem) {
    if (pthread_mutex_lock(&sem->lock) != 0)
        return -1;
    
    while (sem->value <= 0) {
        if (pthread_cond_wait(&sem->cond, &sem->lock) != 0) {
            pthread_mutex_unlock(&sem->lock);
            return -1;
        }
    }
    
    sem->value--;
    pthread_mutex_unlock(&sem->lock);
}