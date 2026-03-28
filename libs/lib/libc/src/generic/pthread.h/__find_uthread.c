#include <pthread.h>
#include <pmos/tls.h>
#include <pmos/__internal.h>

extern int pthread_list_spinlock;
extern struct uthread *worker_thread;

_HIDDEN struct uthread * __find_uthread(uint64_t tid)
{
    if (worker_thread->thread_task_id == tid) {
        return worker_thread;
    }

    pthread_spin_lock(&pthread_list_spinlock);
    struct uthread *u = worker_thread->next;
    while (u != worker_thread) {
        if (u->thread_task_id == tid) {
            pthread_spin_unlock(&pthread_list_spinlock);
            return u;
        }
        u = u->next;
    }
    pthread_spin_unlock(&pthread_list_spinlock);
    return NULL;
}