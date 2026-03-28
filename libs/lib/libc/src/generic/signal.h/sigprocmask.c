#include <signal.h>

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    return pthread_sigmask(how, set, oldset);
}