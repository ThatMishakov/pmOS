#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H 1

#include <sys/uio.h>

#define __DECLARE_SSIZE_T
#define __DECLARE_SIZE_T
#define __DECLARE_SA_FAMILY_T
#include "__posix_types.h"

typedef unsigned long socklen_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    // TODO: Pad for appropriate size.
    // char ss_data[];
};

struct msghdr {
    void * msg_name; //< Optional address.
    socklen_t msg_namelen; //< Size of address.
    struct iovec * msg_iov; //< Scatter/gather array.
    size_t msg_iovlen; //< Members in msg_iov.
    void * msg_control; //< Ancillary data.
    size_t msg_controllen; //< Ancillary data buffer length.
    int msg_flags; //< Flags on received message.
};

struct cmsghdr {
    socklen_t cmsg_len; //< Length of the message, including header.
    int cmsg_level; //< Protocol level of the message.
    int cmsg_type; //< Protocol-specific type of the message.
};

enum {
    SCM_RIGHTS = 0x01, //< Access rights (array of int).
};

/// Returns the pointer to the data portion of a cmsghdr.
#define CMSG_DATA(cmsg) ((void *) ((char *) (cmsg) + sizeof(struct cmsghdr)))

/// Returns the pointer to the next cmsghdr or NULL if the structure is the last one.
#define CMSG_NXTHDR(mhdr, cmsg) \
    ((cmsg)->cmsg_len == 0 ? NULL : \
        (struct cmsghdr *) ((char *) (cmsg) + \
            (((cmsg)->cmsg_len + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))))

/// Returns the pointer to the first cmsghdr in the message or NULL if there is none.
#define CMSG_FIRSTHDR(mhdr) \
    ((mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? \
        (struct cmsghdr *) (mhdr)->msg_control : \
        (struct cmsghdr *) NULL)

struct linger {
    int l_onoff; //< Non-zero to linger on close.
    int l_linger; //< Time to linger.
};

enum {
    SOCK_DGRAM = 1, //< Datagram socket.
    SOCK_RAW = 2, //< Raw-protocol interface.
    SOCK_SEQPACKET = 3, //< Sequenced packet stream.
    SOCK_STREAM = 4, //< Stream socket.
};

enum {
    SOL_SOCKET = 0, //< Socket-level options.
};

enum {
    SO_ACCEPTCONN = 0, //< Reports whether socket listening is enabled.
    SO_BROADCAST = 1, //< Permits sending of broadcast messages.
    SO_DEBUG = 2, //< Enables recording of debugging information.
    SO_DONTRROUTE = 3, //< Disables local routing.
    SO_ERROR = 4, //< Reports and clears error status.
    SO_KEEPALIVE = 5, //< Keeps connections active by enabling the periodic transmission of messages.
    SO_LINGER = 6, //< Lingers on close if data is present.
    SO_OOBINLINE = 7, //< Leaves received out-of-band data (data marked urgent) inline.
    SO_RCVBUF = 8, //< Sets receive buffer size.
    SO_RCVLOWAT = 9, //< Sets the minimum number of bytes to process for socket input operations.
    SO_RCVTIMEO = 10, //< Sets the timeout value that specifies the maximum amount of time an input function waits until it completes.
    SO_REUSEADDR = 11, //< Allows the socket to be bound to an address that is already in use.
    SO_SNDBUF = 12, //< Sets send buffer size.
    SO_SNDLOWAT = 13, //< Sets the minimum number of bytes to process for socket output operations.
    SO_SNDTIMEO = 14, //< Sets the timeout value specifying the amount of time that an output function blocks because flow control prevents data from being sent.
    SO_TYPE = 15, //< Reports the socket type.
};

enum {
    AF_UNSPEC = 0, //< Unspecified.
    AF_INET = 1, //< IPv4 Internet protocols.
    AF_INET6 = 2, //< IPv6 Internet protocols.
    AF_UNIX = 3, //< Unix domain sockets.
};

enum {
    SHUT_RD = 0x1, //< Disables further receive operations.
    SHUT_WR = 0x2, //< Disables further send operations.
    SHUT_RDWR = 0x3, //< Disables further send and receive operations.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int     accept(int, struct sockaddr *restrict, socklen_t *restrict);
int     bind(int, const struct sockaddr *, socklen_t);
int     connect(int, const struct sockaddr *, socklen_t);
int     getpeername(int, struct sockaddr *restrict, socklen_t *restrict);
int     getsockname(int, struct sockaddr *restrict, socklen_t *restrict);
int     getsockopt(int, int, int, void *restrict, socklen_t *restrict);
int     listen(int, int);
ssize_t recv(int, void *, size_t, int);
ssize_t recvfrom(int, void *restrict, size_t, int,
        struct sockaddr *restrict, socklen_t *restrict);
ssize_t recvmsg(int, struct msghdr *, int);
ssize_t send(int, const void *, size_t, int);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *,
        socklen_t);
int     setsockopt(int, int, int, const void *, socklen_t);
int     shutdown(int, int);
int     sockatmark(int);
int     socket(int, int, int);
int     socketpair(int, int, int, int [2]);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_SOCKET_H
