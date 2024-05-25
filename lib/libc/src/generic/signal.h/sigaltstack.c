#include <signal.h>
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <pmos/tls.h>

int sigaltstack(const stack_t *ss, stack_t *oss)
{
    if (ss != NULL) {
        if ((ss->ss_flags & ~SS_DISABLE) != 0) {
            errno = EINVAL;
            return -1;
        }

        if ((ss->ss_flags & SS_DISABLE) && (ss->ss_size < MINSIGSTKSZ)) {
            errno = ENOMEM;
            return -1;
        }
    }

    // Do this here to catch SIGSEGV in case ss is pointing to garbage
    stack_t new_s = ss == NULL ? (stack_t){0} : *ss;
    bool switch_to_new_stack = ss != NULL;

    stack_t old_s = {};
    int result = 0;

    struct uthread *uthread = __get_tls();

    pthread_spin_lock(&uthread->signal_stack_spinlock);

    if (switch_to_new_stack && uthread->signal_stack.ss_flags & SS_ONSTACK) {
        errno = EPERM;
        result = -1;
    } else if (switch_to_new_stack) {
        uthread->signal_stack = new_s;

        // Flip disable bit
        uthread->signal_stack.ss_flags ^= SS_DISABLE;
        uthread->signal_stack.ss_flags &= SS_DISABLE;
    }

    old_s = uthread->signal_stack;
    old_s.ss_flags ^= SS_DISABLE;

    pthread_spin_unlock(&uthread->signal_stack_spinlock);

    if (oss)
        *oss = old_s;

    return result;
}
    