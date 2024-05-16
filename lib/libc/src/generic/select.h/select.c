#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

int select(int nfds, fd_set * readfds,
                  fd_set * writefds,
                  fd_set * exceptfds,
                  struct timeval * timeout)
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