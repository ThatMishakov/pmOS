#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

int sem_post(sem_t *s)
{
    if (s == NULL) {
        errno = EINVAL;
        return -1;
    }

    int r = pthread_mutex_lock(&s->lock);
    if (r < 0)
        return -1;

    s->value++;
    if (s->value <= 1)
        pthread_cond_signal(&s->cond);

    pthread_mutex_unlock(&s->lock);
    return 0;
}