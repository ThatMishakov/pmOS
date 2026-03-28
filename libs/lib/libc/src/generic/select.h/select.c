#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    // Go uses select() for usleep(). Implement enought for that to work
    if (nfds != 0 || readfds != NULL || writefds != NULL || exceptfds != NULL) {
        // Not implemented
        fprintf(stderr, "pmOS libc: select: nfds, readfds, writefds, exceptfds not implemented\n");
        errno = ENOSYS;
        return -1;
    }

    if (timeout == NULL) {
        // Not implemented
        errno = ENOSYS;
        return -1;
    }

    // nanoslpeep
    return usleep(timeout->tv_sec * 1000000 + timeout->tv_usec);
}

int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
    size_t count = 0;
    for (nfds_t i = 0; i < nfds; ++i) {
        if (fds[i].fd == STDOUT_FILENO || fds[i].fd == STDERR_FILENO) {
            count++;
        }
    }

    if (count > 0) {
        return count;
    }

    // Not implemented
    fprintf(stderr, "(args: fds=%p, nfds=%ld, timeout=%d)\n", fds, nfds, timeout);
    fprintf(stderr, "pmOS libc: poll not implemented\n");
    errno = ENOSYS;
    return -1;
}