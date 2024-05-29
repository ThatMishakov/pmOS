#include "fork.h"
#include <pmos/tls.h>
#include <pmos/__internal.h>

extern int pthread_list_spinlock;
extern struct uthread *worker_thread;

_HIDDEN void __fixup_worker_uthread()
{
    // Unlock the spinlock in case some naughty thread locked it in the parent while forking
    __atomic_store_n(&pthread_list_spinlock, 0, __ATOMIC_SEQ_CST);

    worker_thread->next = worker_thread;
    worker_thread->prev = worker_thread;

    worker_thread->thread_task_id = get_task_id();
    worker_thread->cmd_reply_port = INVALID_PORT;
}