#include <pmos/__internal.h>
#include <pmos/tls.h>
#include <pmos/ports.h>
#include <errno.h>
#include <pthread.h>

extern int pthread_list_spinlock;
extern struct uthread *worker_thread;

int __fork_fix_thread(struct uthread *child_uthread, uint64_t thread_tid)
{
    child_uthread->thread_task_id = thread_tid;
    
    ports_request_t r = create_port(thread_tid, 0);
    if (r.result != 0) {
        errno = -r.result;
        return -1;
    }
    child_uthread->cmd_reply_port = r.port;

    pthread_spin_lock(&pthread_list_spinlock);
    child_uthread->next = worker_thread;
    child_uthread->prev = worker_thread->prev;
    worker_thread->prev->next = child_uthread;
    worker_thread->prev = child_uthread;
    pthread_spin_unlock(&pthread_list_spinlock);

    return 0;
}