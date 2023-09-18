#ifndef _POLL_H
#define _POLL_H

struct pollfd {
    int fd; //< file descriptor
    short events; //< requested events
    short revents; //< returned events
};

typedef unsigned long nfds_t;

/// Constants for the 'events' and 'revents' field of struct pollfd
enum {
    POLLIN = 0x001, //< There is data to read
    POLLOUT = 0x002, //< Writing now will not block
    POLLRDNORM = 0x004, //< Normal data may be read
    POLLRDBAND = 0x008, //< Priority data may be read
    POLLPRI = 0x010, //< Priority data may be read
    POLLWRNORM = POLLOUT, //< Writing now will not block
    POLLWRBAND = 0x020, //< Priority data may be written
    POLLNVAL = 0x040, //< Invalid request: fd not open
    POLLHUP = 0x2000, //< Hang up
    POLLERR = 0x4000, //< Error
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _POLL_H