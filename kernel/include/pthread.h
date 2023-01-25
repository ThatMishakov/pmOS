#ifndef _PTHREAD_H
#define _PTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#define PTHREAD_ONCE_INIT 0
typedef unsigned pthread_once_t;
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

#define PTHREAD_MUTEX_INITIALIZER 0
typedef unsigned pthread_mutex_t;
typedef unsigned pthread_mutexattr_t;
int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t*);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

#define PTHREAD_COND_INITIALIZER 0
typedef unsigned pthread_cond_t;
int pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);
int pthread_cond_signal(pthread_cond_t*);

typedef unsigned pthread_key_t;
int pthread_key_create(pthread_key_t* key, void (*)(void*));
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* data);

#ifdef __cplusplus
}
#endif

#endif