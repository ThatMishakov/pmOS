#include <signal.h>
#include <errno.h>

void (*signal(int sig, void (*func)(int)))(int)
{
    struct sigaction old;
    struct sigaction sa = {
        .sa_handler = func,
        .sa_flags = SA_RESTART,
    };

    if (sigaction(sig, &sa, &old) == -1) {
        return SIG_ERR;
    }

    return old.sa_handler;
}