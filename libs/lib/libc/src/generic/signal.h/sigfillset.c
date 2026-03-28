#include <signal.h>
#include <errno.h>
#include <stddef.h>

int sigfillset(sigset_t *set)
{
    if (set == NULL) {
        errno = EINVAL;
        return -1;
    }

    *set = ~0UL;
    return 0;
}