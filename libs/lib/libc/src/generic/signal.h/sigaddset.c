#include <signal.h>
#include <errno.h>
#include <stddef.h>

int sigaddset(sigset_t *set, int signum)
{
    if (set == NULL || signum < 0 || signum >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set |= 1UL << signum;
    return 0;
}