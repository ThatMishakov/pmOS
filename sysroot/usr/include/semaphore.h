#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H 1

#include <time.h>
#include <fcntl.h>

typedef int sem_t;

#define SEM_FAILED ((sem_t *)0)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int    sem_close(sem_t *);
int    sem_destroy(sem_t *);
int    sem_getvalue(sem_t *, int *);
int    sem_init(sem_t *, int, unsigned);
sem_t *sem_open(const char *, int, ...);
int    sem_post(sem_t *);
int    sem_timedwait(sem_t *, const struct timespec *);
int    sem_trywait(sem_t *);
int    sem_unlink(const char *);
int    sem_wait(sem_t *);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SEMAPHORE_H
