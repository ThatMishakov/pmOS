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

#ifndef _LIMITS_H
#define _LIMITS_H

/// Maximum number of operations in a single I/O call
#define AIO_LISTIO_MAX 16
#define _POSIX_AIO_LISTIO_MAX AIO_LISTIO_MAX

/// Maximum number of outstanding asynchronous I/O operations
#define AIO_MAX 16
#define _POSIX_AIO_MAX AIO_MAX

/// Maximum amount by which a process can decrease its asynchronous I/O priority level
#define AIO_PRIO_DELTA_MAX 20

/// Maximum length of argument to the exec functions including environment data
#define ARG_MAX 4096
#define _POSIX_ARG_MAX ARG_MAX

/// Maximum number of functions that may be registered with atexit()
#define ATEXIT_MAX 256

/// Maximum number of simultaneous processes per real user ID
#define CHILD_MAX 256
#define _POSIX_CHILD_MAX CHILD_MAX

/// Maximum number of timer expiration overruns
#define DELAYTIMER_MAX 2147483647
#define _POSIX_DELAYTIMER_MAX DELAYTIMER_MAX

/// Maximum length of a host name
#define HOST_NAME_MAX 64
#define _POSIX_HOST_NAME_MAX HOST_NAME_MAX

/// Maximum number of iovec structures that one process has available for use with readv() or writev()
#define IOV_MAX 1024
#define _XOPEN_IOV_MAX IOV_MAX

/// Maximum length of a login name
#define LOGIN_NAME_MAX 256
#define _POSIX_LOGIN_NAME_MAX LOGIN_NAME_MAX

/// Maximum number of message queues per process
#define MQ_OPEN_MAX 1024
#define _POSIX_MQ_OPEN_MAX MQ_OPEN_MAX

/// Maximum number of supported message priorities
#define MQ_PRIO_MAX 16
#define _POSIX_MQ_PRIO_MAX MQ_PRIO_MAX

/// A value greater than the maximum value that the system may assign to a newly-created file descriptor
#define OPEN_MAX 1024
#define _POSIX_OPEN_MAX OPEN_MAX

/// Size of a page in bytes
#define PAGE_SIZE 4096
#define PAGESIZE PAGE_SIZE

/// Maximum number of attempts to destroy a thread's thread-specific data values on thread exit
#define PTHREAD_DESTRUCTOR_ITERATIONS 8
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

/// Maximum number of data keys per process
#define PTHREAD_KEYS_MAX 1024
#define _POSIX_THREAD_KEYS_MAX PTHREAD_KEYS_MAX

/// Minumum size of a thread's stack
#define PTHREAD_STACK_MIN 16384

/// Maximum number of threads per process
#define PTHREAD_THREADS_MAX (-1U)
#define _POSIX_THREAD_THREADS_MAX PTHREAD_THREADS_MAX

/// Maximum number of realtime signals reserved for application use
#define RTSIG_MAX 32
#define _POSIX_RTSIG_MAX RTSIG_MAX

/// Maximum number of semaphores per process
#define SEM_NSEMS_MAX (-1U)
#define _POSIX_SEM_NSEMS_MAX SEM_NSEMS_MAX

/// Maximum value a semaphore may have
#define SEM_VALUE_MAX 2147483647
#define _POSIX_SEM_VALUE_MAX SEM_VALUE_MAX

/// Maximum number of queued signals that a process may send and have pending at the receiver(s) at any time
#define SIGQUEUE_MAX 32
#define _POSIX_SIGQUEUE_MAX SIGQUEUE_MAX

/// Maximum number of replenishment operations that may be simultaneously pending for a particular sporadic server scheduler
#define SS_REPL_MAX 4
#define _POSIX_SS_REPL_MAX SS_REPL_MAX

/// Maximum number of streams that one process can have open at one time
#define STREAM_MAX 128
#define _POSIX_STREAM_MAX STREAM_MAX

/// Maximum number of symbolic links that can be reliably traversed in the resolution of a pathname in the absence of a loop
#define SYMLOOP_MAX 32
#define _POSIX_SYMLOOP_MAX SYMLOOP_MAX

/// Maximum number of timers per process
#define TIMER_MAX 32
#define _POSIX_TIMER_MAX TIMER_MAX

/// Maximum length of a trace event name
#define TRACE_EVENT_NAME_MAX 256
#define _POSIX_TRACE_EVENT_NAME_MAX TRACE_EVENT_NAME_MAX

/// Maximum number of trace streams that may simultaneously exist in the system
#define TRACE_NAME_MAX 256
#define _POSIX_TRACE_NAME_MAX TRACE_NAME_MAX

/// Maximum number of trace streams that may simultaneously exist in the system
#define TRACE_SYS_MAX (-1U)
#define _POSIX_TRACE_SYS_MAX TRACE_SYS_MAX

/// Maximum number of user trace event IDs
#define TRACE_USER_EVENT_MAX (-1U)
#define _POSIX_TRACE_USER_EVENT_MAX TRACE_USER_EVENT_MAX

/// Maximum length of a terminal device name
#define TTY_NAME_MAX 256
#define _POSIX_TTY_NAME_MAX TTY_NAME_MAX

/// Maximum number of bytes supported for the name of a timezone (not the timezone itself)
#define TZNAME_MAX 256
#define _POSIX_TZNAME_MAX TZNAME_MAX








/// Minimum number of bytes needed to represent the maximum size of a regular file
#define FILESIZEBITS 64

/// Maximum number of links to a single file
#define LINK_MAX 1024
#define _POSIX_LINK_MAX LINK_MAX

/// Maximum number of bytes in a terminal canonical input line
#define MAX_CANON 256
#define _POSIX_MAX_CANON MAX_CANON

/// Minimum number of bytes for which space is available in a terminal input queue; therefore, the maximum number of bytes a conforming application may require to be typed as input before reading them
#define MAX_INPUT 256
#define _POSIX_MAX_INPUT MAX_INPUT

/// Maximum number of bytes in a filename (not including terminating null)
#define NAME_MAX 2047
#define _POSIX_NAME_MAX NAME_MAX

/// Maximum number of bytes the implementation will store as a pathname in a user-supplied buffer of unspecified size, including the terminating null character
#define PATH_MAX 4096
#define _POSIX_PATH_MAX PATH_MAX

/// Maximum number of bytes that are guaranteed to be atomic when writing to a pipe
#define PIPE_BUF 464
#define _POSIX_PIPE_BUF PIPE_BUF

/// Minimum number of bytes for storage allocated for any portion of a file
#define POSIX_ALLOC_SIZE_MIN 512

/// Recommended increment for file transfer sizes between the {pread(), pwrite()} and {read(), write()} functions
#define POSIX_REC_INCR_XFER_SIZE 4096

/// Maximum recommended transfer size
#define POSIX_REC_MAX_XFER_SIZE 131072

/// Minimum recommended file transfer size
#define POSIX_REC_MIN_XFER_SIZE 4096

/// Recommended file transfer buffer alignment
#define POSIX_REC_XFER_ALIGN 16

/// Maximum number of bytes in a symbolic link
#define SYMLINK_MAX 4095
#define _POSIX_SYMLINK_MAX SYMLINK_MAX






/// Maximum obase value allowed by the bc utility
#define BC_BASE_MAX 99
#define _POSIX2_BC_BASE_MAX BC_BASE_MAX

/// Maximum number of elements allowed in an array by the bc utility
#define BC_DIM_MAX 2048
#define _POSIX2_BC_DIM_MAX BC_DIM_MAX

/// Maximum scale value allowed by the bc utility
#define BC_SCALE_MAX 99
#define _POSIX2_BC_SCALE_MAX BC_SCALE_MAX

/// Maximum length of a string constant accepted by the bc utility
#define BC_STRING_MAX 1000
#define _POSIX2_BC_STRING_MAX BC_STRING_MAX

/// Maximum number of bytes in a character class name
#define CHARCLASS_NAME_MAX 2048
#define _POSIX2_CHARCLASS_NAME_MAX CHARCLASS_NAME_MAX

/// Maximum number of weights that can be assigned to an entry of the LC_COLLATE order keyword in the locale definition file; see {LC_COLLATE}
#define COLL_WEIGHTS_MAX 8
#define _POSIX2_COLL_WEIGHTS_MAX COLL_WEIGHTS_MAX

/// Maximum number of expressions that can be nested within parentheses by the expr utility
#define EXPR_NEST_MAX 32
#define _POSIX2_EXPR_NEST_MAX EXPR_NEST_MAX

/// Maximum length, in bytes, of an input line
#define LINE_MAX 2048
#define _POSIX2_LINE_MAX LINE_MAX

/// Maximum number of supplementary group IDs per process
#define NGROUPS_MAX 65536
#define _POSIX2_NGROUPS_MAX NGROUPS_MAX

/// Maximum number of repeated occurrences of a BRE permitted by the {sed} utility when using the interval notation \{M,N\}
#define RE_DUP_MAX 255
#define _POSIX2_RE_DUP_MAX RE_DUP_MAX





/// The resolution in nanoseconds of the CLOCK_REALTIME clock
#define _POSIX_CLOCKRES_MIN 20000000




/// Number of bits in a type char
#define CHAR_BIT 8

/// Maximum value of a char
#define CHAR_MAX 127

/// Minimum value of a char
#define CHAR_MIN (-128)

/// Maximum value of int
#define INT_MAX 2147483647

/// Minimum value of int
#define INT_MIN (-2147483648)

/// Maximum value of long long
#define LLONG_MAX 9223372036854775807LL

/// Minimum value of long long
#define LLONG_MIN (-9223372036854775808LL)

/// Number of bits in a type long
#define LONG_BIT 64

/// Maximum value of long
#define LONG_MAX 9223372036854775807L

/// Minimum value of long
#define LONG_MIN (-9223372036854775808L)

/// Maximum bytes in a multibyte character, for any locale
#define MB_LEN_MAX 4

/// Maximum value of signed char
#define SCHAR_MAX 127

/// Minimum value of signed char
#define SCHAR_MIN (-128)

/// Maximum value of short
#define SHRT_MAX 32767

/// Minimum value of short
#define SHRT_MIN (-32768)

/// Maximum value of ssize_t
#define SSIZE_MAX 9223372036854775807L

/// Maximum value of unsigned char
#define UCHAR_MAX 255

/// Maximum value of unsigned int
#define UINT_MAX 4294967295U

/// Maximum value of unsigned long
#define ULONG_MAX 18446744073709551615UL

/// Maximum value of unsigned long long
#define ULLONG_MAX 18446744073709551615ULL

/// Maximum value of unsigned short
#define USHRT_MAX 65535

/// Maximum value of size_t
#define SIZE_MAX 18446744073709551615UL


/// Number of bytes in int
#define WORD_BIT 32




/// Maximum number of n in conversion specifications using the "%n$" sequence in printf() or scanf()
#define NL_ARGMAX 255

/// Maximum number of bytes in a lang name
#define NL_LANGMAX 32

/// Maximum message number
#define NL_MSGMAX 18446744073709551615UL

/// Maximum set number
#define NL_SETMAX 65535

/// Maximum number of bytes in a message string
#define NL_TEXTMAX 65535
#define _POSIX2_LINE_MAX NL_TEXTMAX

/// Default process priority
// TODO: This is not correct
#define NZERO 20

#endif