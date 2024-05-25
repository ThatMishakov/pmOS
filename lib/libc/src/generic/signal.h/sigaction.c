#include <signal.h>
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "sigaction.h"

struct sigaction __sigactions[NSIG] = {};
int __sigaction_lock = 0;

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    if (signum < 0 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    struct sigaction old_action;
    struct sigaction new_action = act ? *act : (struct sigaction){0};
    bool replace = act != NULL;

    pthread_spin_lock(&__sigaction_lock);

    old_action = __sigactions[signum];
    if (replace) {
        __sigactions[signum] = new_action;
    }

    pthread_spin_unlock(&__sigaction_lock);

    if (oldact) {
        *oldact = old_action;
    }

    return 0;
}