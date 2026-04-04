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

#ifndef _ERRNO_H
#define _ERRNO_H

#if defined(__cplusplus)
extern "C" {
#endif

// Fun fact: gcc/libgo/mksysinfo.sh doesn't like errno constants with parentheses

// C signals
#define ERANGE 1
#define EILSEQ 2
#define ERANGE 3

// TODO: Fix this macro
// #ifdef _POSIX_C_SOURCE

/// Argument list too long
#define E2BIG 4

/// Permission denied
#define EACCES 5

/// Address in use
#define EADDRINUSE 6

/// Address not available
#define EADDRNOTAVAIL 7

/// Address family not supported
#define EAFNOSUPPORT 8

/// Recource not available, try again
#define EAGAIN 9

/// Connection already in progress
#define EALREADY 10

/// Bad file descriptor
#define EBADF 11

/// Bad message
#define EBADMSG 12

/// Device or recource busy
#define EBUSY 13

/// Operation cancelled
#define ECANCELED 14

/// No child process
#define ECHILD 15

/// Connection aborted
#define ECONNABORTED 16

/// Connection refused
#define ECONNREFUSED 17

/// Connection reset
#define ECONNRESET 18

/// Recource deadlock will be caused
#define EDEADLK 19

/// Destination address required
#define EDESTADDRREQ 20

/// Argument out of domain function
#define EDOM 21

/// Reserved
#define EDQUOT 22

/// File exists
#define EEXIST 23

/// Bad address
#define EFAULT 24

/// File too large
#define EFBIG 25

/// Host is unreachable
#define EHOSTUNREACH 26

/// Identifier removed
#define EIDRM 27

/// Illegal byte sequence
#define EILSEQ 28

/// Operation in progress
#define EINPROGRESS 29

/// Interrupted function
#define EINTR 30

/// Invalid argument
#define EINVAL 31

/// I/O error
#define EIO 32

/// Socket is connected
#define EISCONN 33

/// Is a directory
#define EISDIR 34

/// Too many levels of symbolic links
#define ELOOP 35

/// File descruptor value too large
#define EMFILE 36

/// Too many links
#define EMLINK 37

/// Message too large
#define EMSGSIZE 38

/// Reserved
#define EMULTIHOP 39

/// File name too long
#define ENAMETOOLONG 40

/// Network is down
#define ENETDOWN 41

/// Connection aborted by network
#define ENETRESET 42

/// Network unreachable
#define ENETUNREACH 43

/// Too many files open
#define ENFILE 44

/// No buffer space available
#define ENOBUFS 45

/*

#define ENODATA (46)
*/

/// No such device
#define ENODEV 47

/// No such file or directory
#define ENOENT 48

/// Executable format error
#define ENOEXEC 49

/// No locks available
#define ENOLCK 50

/// Reserved
#define ENOLCK 51

/// Not enough space
#define ENOMEM 52

/// No message of the desired type
#define ENOMSG 53

/// Protocol not available
#define ENOPROTOOPT 54

/// No space left on device
#define ENOSPC 55

/*

#define ENOSR (56)
#define ENOSTR (57)

*/

/// Functionality not supported
#define ENOSYS 58

/// Socket is not connected
#define ENOTCONN 59

/// Not a directory or a symbolic link to a directory
#define ENOTDIR 60

/// Directory not empty
#define ENOTEMPTY 61

/// State not recoverable
#define ENOTRECOVERABLE 62

/// Not a socket
#define ENOTSOCK 63

/// Not supported
#define ENOTSUP 64

/// Inappropriate I/O operation
#define ENOTTY 65

/// No such device or address
#define ENXIO 66

/// Operation not supported on socket
#define EOPNOTSUPP 67

/// Value too large to be stored in data type
#define EOVERFLOW 68

/// Previous owner died
#define EOWNERDEAD 69

/// Operation not permitted
#define EPERM 70

/// Broken pipe
#define EPIPE 71

/// Protocol error
#define EPROTO 72

/// Protocol not supported
#define EPROTONOSUPPORT 73

/// Protocol weong type for socket
#define EPROTOTYPE 74

/// Result too large
#define ERANGE 75

/// Read-only file system
#define EROFS 76

/// Invalid seek
#define ESPIPE 77

/// No such process
#define ESRCH 78

/// Reserved
#define ESTALE 79

/// Connection timed out
#define ETIMEDOUT 80

/// Text file busy
#define ETXTBSY 81

/// Operation would block
#define EWOULDBLOCK 82

/// Cross-device link
#define EXDEV 83

// #endif // _POSIX_C_SOURCE

#ifdef __STDC_HOSTED__

/// This function returns the pointer to thread-local __errno variable. The program might redefine
/// this function if thread-local storage is not supported.
extern int *__get_errno();

#endif

#define errno (*__get_errno())

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _ERRNO_H */