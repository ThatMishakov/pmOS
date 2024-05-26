#include <pthread.h>
#include <stdio.h>
#include <pmos/tls.h>
#include <pmos/system.h>

extern int pthread_list_spinlock;
// Worker thread is the list head
extern struct uthread *worker_thread;
extern int no_new_threads;

void __terminate_threads()
{
    // Terminate all threads
    pthread_spin_lock(&pthread_list_spinlock);
    no_new_threads = 1;
    struct uthread *u = worker_thread->next;
    while (u != worker_thread) {
        struct uthread *next = u->next;
        syscall_kill_task(u->thread_task_id);
        u = next;
    }
    pthread_spin_unlock(&pthread_list_spinlock);
}
    