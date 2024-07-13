#include <signal.h>
#include <pmos/tls.h>
#include <errno.h>
#include <pmos/ipc.h>

extern pmos_port_t worker_port;

uint64_t global_pending_signals = 0;

int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    printf("pthread_sigmask %i %lx %lx\n", how, set, oldset);

    struct uthread *u = __get_tls();

    if (oldset != NULL)
        *oldset = u->sigmask;

    if (set != NULL) {
        uint64_t new_mask;
        switch (how) {
            case SIG_BLOCK:
                new_mask = __atomic_or_fetch(&u->sigmask, *set, __ATOMIC_RELAXED);
                break;
            case SIG_UNBLOCK:
                new_mask = __atomic_and_fetch(&u->sigmask, ~(*set), __ATOMIC_RELAXED);
                break;
            case SIG_SETMASK:
                __atomic_store_n(&u->sigmask, *set, __ATOMIC_RELAXED);
                new_mask = *set;
                break;
            default:
                errno = EINVAL;
                return -1;
        }

        uint64_t global_pending = __atomic_load_n(&global_pending_signals, __ATOMIC_SEQ_CST);
        uint64_t thread_pending = __atomic_load_n(&u->pending_signals, __ATOMIC_RELAXED);

        if ((new_mask & global_pending) != 0 || (new_mask & thread_pending) != 0) {
            IPC_Refresh_Signals msg = {
                .type = IPC_Refresh_Signals_NUM,
                .flags = 0,
            };

            send_message_port(worker_port, sizeof(msg), &msg);
        }
    }

    return 0;
}