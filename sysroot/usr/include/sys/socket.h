/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H 1

#include <sys/uio.h>

#define __DECLARE_SSIZE_T
#define __DECLARE_SIZE_T
#define __DECLARE_SA_FAMILY_T
#define __DECLARE_OFF_T
#include "__posix_types.h"

// TODO: This is what Go expects, but I prefer it to be 64-bit.
typedef unsigned socklen_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char ss_data[254];
};

struct msghdr {
    void *msg_name;        //< Optional address.
    socklen_t msg_namelen; //< Size of address.
    struct iovec *msg_iov; //< Scatter/gather array.
    size_t msg_iovlen;     //< Members in msg_iov.
    void *msg_control;     //< Ancillary data.
    size_t msg_controllen; //< Ancillary data buffer length.
    int msg_flags;         //< Flags on received message.
};

struct cmsghdr {
    socklen_t cmsg_len; //< Length of the message, including header.
    int cmsg_level;     //< Protocol level of the message.
    int cmsg_type;      //< Protocol-specific type of the message.
};

enum {
    SCM_RIGHTS = 0x01, //< Access rights (array of int).
};

/// Returns the pointer to the data portion of a cmsghdr.
#define CMSG_DATA(cmsg) ((void *)((char *)(cmsg) + sizeof(struct cmsghdr)))

/// Returns the pointer to the next cmsghdr or NULL if the structure is the last one.
#define CMSG_NXTHDR(mhdr, cmsg)                \
    ((cmsg)->cmsg_len == 0                     \
         ? NULL                                \
         : (struct cmsghdr *)((char *)(cmsg) + \
                              (((cmsg)->cmsg_len + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))))

/// Returns the pointer to the first cmsghdr in the message or NULL if there is none.
#define CMSG_FIRSTHDR(mhdr)                                                                   \
    ((mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? (struct cmsghdr *)(mhdr)->msg_control \
                                                      : (struct cmsghdr *)NULL)

struct linger {
    int l_onoff;  //< Non-zero to linger on close.
    int l_linger; //< Time to linger.
};

enum {
    SOCK_DGRAM     = 1, //< Datagram socket.
    SOCK_RAW       = 2, //< Raw-protocol interface.
    SOCK_SEQPACKET = 3, //< Sequenced packet stream.
    SOCK_STREAM    = 4, //< Stream socket.
};

enum {
    SOL_SOCKET = 0, //< Socket-level options.
};

enum {
    SO_ACCEPTCONN = 0, //< Reports whether socket listening is enabled.
    SO_BROADCAST  = 1, //< Permits sending of broadcast messages.
    SO_DEBUG      = 2, //< Enables recording of debugging information.
    SO_DONTRROUTE = 3, //< Disables local routing.
    SO_ERROR      = 4, //< Reports and clears error status.
    SO_KEEPALIVE =
        5, //< Keeps connections active by enabling the periodic transmission of messages.
    SO_LINGER    = 6,  //< Lingers on close if data is present.
    SO_OOBINLINE = 7,  //< Leaves received out-of-band data (data marked urgent) inline.
    SO_RCVBUF    = 8,  //< Sets receive buffer size.
    SO_RCVLOWAT  = 9,  //< Sets the minimum number of bytes to process for socket input operations.
    SO_RCVTIMEO  = 10, //< Sets the timeout value that specifies the maximum amount of time an input
                       //function waits until it completes.
    SO_REUSEADDR = 11, //< Allows the socket to be bound to an address that is already in use.
    SO_SNDBUF    = 12, //< Sets send buffer size.
    SO_SNDLOWAT  = 13, //< Sets the minimum number of bytes to process for socket output operations.
    SO_SNDTIMEO  = 14, //< Sets the timeout value specifying the amount of time that an output
                       //function blocks because flow control prevents data from being sent.
    SO_TYPE      = 15, //< Reports the socket type.
};

enum {
    AF_UNSPEC = 0, //< Unspecified.
    AF_INET   = 1, //< IPv4 Internet protocols.
    AF_INET6  = 2, //< IPv6 Internet protocols.
    AF_UNIX   = 3, //< Unix domain sockets.
};

enum {
    SHUT_RD   = 0x1, //< Disables further receive operations.
    SHUT_WR   = 0x2, //< Disables further send operations.
    SHUT_RDWR = 0x3, //< Disables further send and receive operations.
};

enum {
    SOMAXCONN = 1024, //< Maximum number of connections.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int accept(int, struct sockaddr *, socklen_t *);
int bind(int, const struct sockaddr *, socklen_t);
int connect(int, const struct sockaddr *, socklen_t);
int getpeername(int, struct sockaddr *, socklen_t *);
int getsockname(int, struct sockaddr *, socklen_t *);
int getsockopt(int, int, int, void *, socklen_t *);
int listen(int, int);
ssize_t recv(int, void *, size_t, int);
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t recvmsg(int, struct msghdr *, int);
ssize_t send(int, const void *, size_t, int);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
int setsockopt(int, int, int, const void *, socklen_t);
int shutdown(int, int);
int sockatmark(int);
int socket(int, int, int);
int socketpair(int, int, int, int[2]);

/**
 * @brief Accept an incoming network connection with additional options.
 *
 * The `accept4` function is used to accept an incoming network connection on a
 * socket. It provides additional options for controlling the behavior of the
 * accepted socket, such as setting flags for the socket.
 *
 * @param sockfd    The socket file descriptor on which to accept the connection.
 * @param addr      A pointer to a sockaddr structure to store the address of the
 *                  connecting peer. Pass NULL if you don't need the peer's address.
 * @param addrlen   A pointer to an integer that specifies the size of the `addr`
 *                  structure. This should be initialized to the size of the `addr`
 *                  structure before calling `accept4`.
 * @param flags     Additional flags to control the behavior of the accepted socket.
 * @return          If successful, the function returns a new socket file descriptor
 *                  representing the accepted connection. On error, it returns -1 and
 *                  sets `errno` to indicate the error.
 *
 * @note            The `accept4` function is typically used with non-blocking sockets
 *                  and can be used to set socket options like O_NONBLOCK and O_CLOEXEC.
 */
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);

/**
 * @brief Accept an incoming network connection with additional options (POSIX version).
 *
 * The `paccept` function is used to accept an incoming network connection on a socket.
 * It provides additional options for controlling the behavior of the accepted socket
 * and is a POSIX version of the `accept4` function.
 *
 * @param sockfd    The socket file descriptor on which to accept the connection.
 * @param addr      A pointer to a sockaddr structure to store the address of the
 *                  connecting peer. Pass NULL if you don't need the peer's address.
 * @param addrlen   A pointer to an integer that specifies the size of the `addr`
 *                  structure. This should be initialized to the size of the `addr`
 *                  structure before calling `paccept`.
 * @param sigmask   A pointer to a signal mask that specifies the set of signals to
 *                  be blocked while waiting for a connection. Pass NULL for no signal
 *                  blocking.
 * @param flags     Additional flags to control the behavior of the accepted socket.
 * @return          If successful, the function returns a new socket file descriptor
 *                  representing the accepted connection. On error, it returns -1 and
 *                  sets `errno` to indicate the error.
 *
 * @note            The `paccept` function is typically used with non-blocking sockets
 *                  and can be used to set socket options like O_NONBLOCK and O_CLOEXEC.
 *                  It also allows specifying a signal mask to block certain signals
 *                  while waiting for a connection.
 */
int paccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, const sigset_t *sigmask,
            int flags);

/**
 * @brief Send a file to a socket.
 *
 * This function sends a regular file or shared memory object specified by the
 * file descriptor @p fd out to a stream socket specified by descriptor @p s.
 *
 * @param fd      The file descriptor of the file to send.
 * @param s       The descriptor of the socket to send the file to.
 * @param offset  The offset in the file where to begin sending.
 * @param nbytes  The number of bytes of the file to send. Use 0 to send until
 *                the end of the file is reached.
 * @param hdtr    An optional header and/or trailer to send before and after
 *                the file data. See struct sf_hdtr for details.
 * @param sbytes  If non-NULL, the total number of bytes sent is stored here.
 * @param flags   A bitmap of flags that control the behavior of sendfile.
 *
 * @return 0 on success, or -1 on error with errno set to indicate the error.
 *
 * @note This function is not standard and is only generally present on BSD systems.
 *       Since pmOS is not BSD, its behavior might be slightly different.
 */
int sendfile(int fd, int s, off_t offset, size_t nbytes, struct sf_hdtr *hdtr, off_t *sbytes,
             int flags);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_SOCKET_H
