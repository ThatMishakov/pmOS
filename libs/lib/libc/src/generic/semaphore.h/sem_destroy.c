#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

int sem_destroy(sem_t *s)
{    
    if (s == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (pthread_mutex_lock(&s->lock) < 0)
        return -1;

    if (s->value < 0) {
        // Threads are blocking on the semaphore
        pthread_mutex_unlock(&s->lock);
        errno = EBUSY;
        return -1;
    }

    pthread_mutex_unlock(&s->lock);

    pthread_mutex_destroy(&s->lock);
    pthread_cond_destroy(&s->cond);
    return 0;
}