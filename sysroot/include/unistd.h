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

#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define __DECLARE_PID_T
#define __DECLARE_UID_T
#define __DECLARE_GID_T
#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#define __DECLARE_OFF_T
#include "__posix_types.h"

#define _POSIX_VERSION  200809L
#define _POSIX2_VERSION 200809L
#define _XOPEN_VERSION  700

#define _POSIX_MESSAGE_PASSING       200809L
#define _POSIX_RAW_SOCKETS           200809L
#define _POSIX_SHARED_MEMORY_OBJECTS 200809L

/// Asynchronous input and output.
#define _POSIX_ASYNCHRONOUS_IO 200809L
/// Barrier synchronization methods.
#define _POSIX_BARRIERS 200809L
/// The use of chown() and fchown() is restricted to a process with appropriate privileges.
#define _POSIX_CHOWN_RESTRICTED 200809L
/// Clock selection.
#define _POSIX_CLOCK_SELECTION 200809L
/// Process CPU-Time Clocks.
// #define _POSIX_CPUTIME 200809L
/// File synchronisation.
#define _POSIX_FSYNC 200809L
/// IPv6 support.
#define _POSIX_IPV6 200809L
/// Job control.
#define _POSIX_JOB_CONTROL 200809L
/// Memory mapped files.
#define _POSIX_MAPPED_FILES 200809L
/// Process memory locking.
// #define _POSIX_MEMLOCK 200809L
/// Range memory locking.
// #define _POSIX_MEMLOCK_RANGE 200809L
/// Process memory protection.
#define _POSIX_MEMORY_PROTECTION 200809L
/// Message passing.
#define _POSIX_MESSAGE_PASSING 200809L
/// Monotonic clock.
// #define _POSIX_MONOTONIC_CLOCK 200809L
/// Pathnames longer than {NAME_MAX} generate an error.
#define _POSIX_NO_TRUNC 200809L
/// Prioritized input and output.
// #define _POSIX_PRIORITIZED_IO 200809L
/// Process scheduling.
// #define _POSIX_PRIORITY_SCHEDULING 200809L
/// Raw sockets.
#define _POSIX_RAW_SOCKETS 200809L
/// Read-write locks.
#define _POSIX_READER_WRITER_LOCKS 200809L
/// Realtime signals.
#define _POSIX_REALTIME_SIGNALS 200809L
/// Regular expression handling.
#define _POSIX_REGEXP 200809L
/// Each process has a saved set-user-ID and a saved set-group-ID
#define _POSIX_SAVED_IDS 200809L
/// Semaphores
#define _POSIX_SEMAPHORES 200809L
/// Shared Memory Objects
#define _POSIX_SHARED_MEMORY_OBJECTS 200809L
/// POSIX shell
#define _POSIX_SHELL 200809L
/// POSIX spawn
#define _POSIX_SPAWN 200809L
/// Spin Locks
#define _POSIX_SPIN_LOCKS 200809L
/// Process Sporadic Server
// #define _POSIX_SPORADIC_SERVER 200809L
/// Synchronized I/O
// #define _POSIX_SYNCHRONIZED_IO 200809L
/// Thread Stack Address Attribute
#define _POSIX_THREAD_ATTR_STACKADDR 200809L
/// Thread Stack Size Attribute
#define _POSIX_THREAD_ATTR_STACKSIZE 200809L
/// Thread CPU-Time Clock options
// #define _POSIX_THREAD_CPUTIME 200809L
/// Non-Robust Mutex Priority Inheritance
// #define _POSIX_THREAD_PRIO_INHERIT 200809L
/// Non-Robust Mutex Priority Protection
// #define _POSIX_THREAD_PRIO_PROTECT 200809L
/// Thread Execution Scheduling
// #define _POSIX_THREAD_PRIORITY_SCHEDULING 200809L
/// Thread Process-Shared Synchronization
#define _POSIX_THREAD_PROCESS_SHARED 200809L
/// Thread Robust Mutex Priority Inheritance
// #define _POSIX_THREAD_ROBUST_PRIO_INHERIT 200809L
/// Thread Robust Mutex Priority Protection
// #define _POSIX_THREAD_ROBUST_PRIO_PROTECT 200809L
/// Thread Safe Functions
#define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
/// Thread Sporadic Server
// #define _POSIX_THREAD_SPORADIC_SERVER 200809L
/// Threads support
#define _POSIX_THREADS 200809L
/// Timeouts for Threads
#define _POSIX_TIMEOUTS 200809L
/// Timers
#define _POSIX_TIMERS 200809L
/// Trace
// #define _POSIX_TRACE 200809L
/// Trace Event Filter
// #define _POSIX_TRACE_EVENT_FILTER 200809L
/// Trace Inherit
// #define _POSIX_TRACE_INHERIT 200809L
/// Trace Log
// #define _POSIX_TRACE_LOG 200809L
/// Typed Memory Objects
#define _POSIX_TYPED_MEMORY_OBJECTS 200809L

/// 32 bit int and 64 bit long, pointer and off_t
#define _POSIX_V6_LP64_OFF64 200809L
#define _POSIX_V7_LP64_OFF64 200809L

/// C-Binding option
#define _POSIX2_C_BIND 200809L
/// C-Language Development Utilities
// #define _POSIX2_C_DEV 200809L

/// Terminal characteristics
#define _POSIX2_CHAR_TERM 200809L

/// Fortran Development Utilities
// #define _POSIX2_FORT_DEV 200809L

/// Fortran Runtime Utilities
#define _POSIX2_FORT_RUN 200809L

/// Localedef
// #define _POSIX2_LOCALEDEF 200809L

/// Batch Environment Services
// #define _POSIX2_PBS 200809L
/// Batch Accounting Services
// #define _POSIX2_PBS_ACCOUNTING 200809L
/// Batch Checkpoint/Restart
// #define _POSIX2_PBS_CHECKPOINT 200809L
/// Locate Batch Job Request
// #define _POSIX2_PBS_LOCATE 200809L
/// Batch Job Message Request
// #define _POSIX2_PBS_MESSAGE 200809L
/// Software Development Utilities
// #define _POSIX2_SW_DEV 200809L
/// User Portability Utilities
// #define _POSIX2_UPE 200809L
/// X/Open Encryption Option
// #define _XOPEN_CRYPT 200809L
/// X/Open Realtime
// #define _XOPEN_REALTIME 200809L
/// X/Open Realtime Threads
// #define _XOPEN_REALTIME_THREADS 200809L
/// Shared Memory Open Group
#define _XOPEN_SHM 200809L
/// X/Open STREAMS
#define _XOPEN_STREAMS 200809L
/// XSI option
#define _XOPEN_XSI 200809L
/// UUCP utilities
// #define _XOPEN_UUCP 200809L

/// Asynchronous input and output
#define _POSIX_ASYNC_IO 0x1
/// Prioritized input and output
#define _POSIX_PRIO_IO 0x2
/// Synchonized input and output
#define _POSIX_SYNC_IO 0x4

/// The resolution in nanoseconds of all file timestamps.
#define _POSIX_TIMESTAMP_RESOLUTION 0x1000000
/// Symbolic links are supported.
#define _POSIX2_SYMLINKS 0x2000000

/// Test for existence of file
#define F_OK 0
/// Test for read permission
#define R_OK 0x1
/// Test for write permission
#define W_OK 0x2
/// Test for execute permission
#define X_OK 0x4

#define _CS_PATH                           0x1
#define _CS_POSIX_V7_ILP32_OFF32_CFLAGS    0x2
#define _CS_POSIX_V7_ILP32_OFF32_LDFLAGS   0x3
#define _CS_POSIX_V7_ILP32_OFF32_LIBS      0x4
#define _CS_POSIX_V7_ILP32_OFFBIG_CFLAGS   0x5
#define _CS_POSIX_V7_ILP32_OFFBIG_LDFLAGS  0x6
#define _CS_POSIX_V7_ILP32_OFFBIG_LIBS     0x7
#define _CS_POSIX_V7_LP64_OFF64_CFLAGS     0x8
#define _CS_POSIX_V7_LP64_OFF64_LDFLAGS    0x9
#define _CS_POSIX_V7_LP64_OFF64_LIBS       0xa
#define _CS_POSIX_V7_LPBIG_OFFBIG_CFLAGS   0xb
#define _CS_POSIX_V7_LPBIG_OFFBIG_LDFLAGS  0xc
#define _CS_POSIX_V7_LPBIG_OFFBIG_LIBS     0xd
#define _CS_POSIX_V7_THREADS_CFLAGS        0xe
#define _CS_POSIX_V7_THREADS_LDFLAGS       0xf
#define _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS 0x10
#define _CS_V7_ENV                         0x11

/// function argument for flock()
/// @see flock()
enum {
    F_LOCK = 0, //< Lock a file region for exclusive use.
    F_TEST = 1, //< Test a lock.
    F_TLOCK =
        2, //< Lock a file region for exclusive use, but return if the region is already locked.
    F_ULOCK = 3 //< Unlock a file region.
};

/// Function constants for pathconf() and fpathconf()
/// @see pathconf()
/// @see fpathconf()
enum {
    _PC_FILESIZEBITS = 0, //< Number of bits used to store file size.
    _PC_LINK_MAX     = 1, //< Maximum number of links to a file.
    _PC_MAX_CANON    = 2, //< Maximum length of a canonical input line.
    _PC_MAX_INPUT    = 3, //< Maximum length of an input line.
    _PC_NAME_MAX     = 4, //< Maximum length of a filename.
    _PC_PATH_MAX     = 5, //< Maximum length of a relative pathname.
    _PC_PIPE_BUF     = 6, //< Maximum number of bytes that can be written atomically to a pipe.
    _PC_2_SYMLINKS = 7, //< Maximum number of symbolic links that can be traversed in the resolution
                        //of a pathname in the absence of a loop.
    _PC_ALLOC_SIZE_MIN = 8, //< Minimum size for a file containing a hole.
    _PC_REC_MAX_XFER_SIZE =
        9, //< Recommended increment for file transfer sizes between the processes.
    _PC_REC_MIN_XFER_SIZE =
        10, //< Recommended increment for file transfer sizes between the processes.
    _PC_REC_XFER_ALIGN = 11, //< Recommended file transfer buffer alignment.
    _PC_SYMLINK_MAX    = 12, //< Maximum length of a symbolic link.
    _PC_CHOWN_RESTRICTED =
        13, //< Whether chown requests are restricted to processes with appropriate privileges.
    _PC_NO_TRUNC =
        14, //< Whether attempts to create a pathname longer than {NAME_MAX} generate an error.
    _PC_VDISABLE             = 15, //< Terminal special characters can be disabled.
    _PC_ASYNC_IO             = 16, //< Asynchronous input and output is supported.
    _PC_PRIO_IO              = 17, //< Prioritized input and output is supported.
    _PC_SYNC_IO              = 18, //< Synchronized input and output is supported.
    _PC_TIMESTAMP_RESOLUTION = 19, //< The resolution in nanoseconds of all file timestamps.
};

/// Function constants for sysconf()
/// @see sysconf()
enum {
    _SC_AIO_LISTIO_MAX = 1, //< Maximum number of I/O operations in a list I/O call.
    _SC_AIO_MAX        = 2, //< Maximum number of outstanding asynchronous I/O operations.
    _SC_AIO_PRIO_DELTA_MAX =
        3, //< Maximum amount by which a process can decrease its asynchronous I/O priority level.
    _SC_ARG_MAX       = 4,  //< Maximum length of the arguments to the exec() family of functions.
    _SC_ATEXIT_MAX    = 5,  //< Maximum number of functions that may be registered with atexit().
    _SC_BC_BASE_MAX   = 6,  //< Maximum obase values allowed by the bc utility.
    _SC_BC_DIM_MAX    = 7,  //< Maximum number of elements permitted in an array by the bc utility.
    _SC_BC_SCALE_MAX  = 8,  //< Maximum scale value allowed by the bc utility.
    _SC_BC_STRING_MAX = 9,  //< Maximum length of a string constant accepted by the bc utility.
    _SC_CHILD_MAX     = 10, //< Maximum number of simultaneous processes per real user ID.
    _SC_CLK_TCK       = 11, //< Number of clock ticks per second.
    _SC_COLL_WEIGHTS_MAX = 12, //< Maximum number of weights that can be assigned to an entry of the
                               //LC_COLLATE order keyword in the locale definition file.
    _SC_DELAYTIMER_MAX   = 13, //< Maximum number of timer expiration overruns.
    _SC_EXPR_NEST_MAX = 14, //< Maximum number of expressions that can be nested within parentheses
                            //by the expr utility.
    _SC_HOST_NAME_MAX = 15, //< Maximum length of a host name (not including the terminating null)
                            //as returned from the gethostname() function.
    _SC_IOV_MAX  = 16, //< Maximum number of iovec structures that one process has available for use
                       //with readv() or writev().
    _SC_LINE_MAX = 17, //< Maximum length of a utility's input line.
    _SC_LOGIN_NAME_MAX = 18, //< Maximum length of a login name.
    _SC_NGROUPS_MAX    = 19, //< Maximum number of simultaneous supplementary group IDs per process.
    _SC_GETGR_R_SIZE_MAX =
        20, //< Initial size of the array used by the getgrnam() and getgrgid() functions.
    _SC_GETPW_R_SIZE_MAX =
        21, //< Initial size of the array used by the getpwnam_r() and getpwuid_r() functions.
    _SC_MQ_OPEN_MAX = 22, //< Maximum number of message queues open for a process.
    _SC_MQ_PRIO_MAX = 23, //< Maximum number of supported message priorities.
    _SC_OPEN_MAX  = 24, //< Maximum number of files that one process can have open at any one time.
    _SC_PAGE_SIZE = 25, //< Size of a page in bytes.
    _SC_PAGESIZE  = 25, //< Size of a page in bytes.
    _SC_THREAD_DESTRUCTOR_ITERATIONS = 26, //< Maximum number of attempts made to destroy a thread's
                                           //thread-specific data values on thread exit.
    _SC_THREAD_KEYS_MAX              = 27, //< Maximum number of data keys per process.
    _SC_THREAD_STACK_MIN             = 28, //< Minimum size in bytes of thread stack storage.
    _SC_THREAD_THREADS_MAX           = 29, //< Maximum number of threads per process.
    _SC_RE_DUP_MAX    = 30, //< Maximum number of repeated occurrences of a regular expression
                            //permitted when using the interval notation \{m,n\}.
    _SC_RTSIG_MAX     = 31, //< Maximum number of realtime signals reserved for application use.
    _SC_SEM_NSEMS_MAX = 32, //< Maximum number of semaphores that a process may have.
    _SC_SEM_VALUE_MAX = 33, //< Maximum value a semaphore may have.
    _SC_SIGQUEUE_MAX  = 34, //< Maximum number of queued signals that a process may send and have
                            //pending at the receiver(s) at any time.
    _SC_STREAM_MAX   = 35, //< Maximum number of streams that one process can have open at one time.
    _SC_SYMLOOP_MAX  = 36, //< Maximum number of symbolic links that can be traversed in the
                           //resolution of a pathname in the absence of a loop.
    _SC_TIMER_MAX    = 37, //< Maximum number of timers per process supported by the implementation.
    _SC_TTY_NAME_MAX = 38, //< Maximum length of terminal device name.
    _SC_TZNAME_MAX   = 39, //< Maximum length of a time zone name (element of tzname).
    _SC_ADVISORY_INFO       = 40, //< Advisory information.
    _SC_BARRIERS            = 41, //< Barrier synchronization methods.
    _SC_ASYNCHRONOUS_IO     = 42, //< Asynchronous input and output.
    _SC_CLOCK_SELECTION     = 43, //< Clock selection.
    _SC_CPUTIME             = 44, //< Process CPU-Time Clocks.
    _SC_FSYNC               = 45, //< File synchronisation.
    _SC_IPV6                = 46, //< IPv6 support.
    _SC_JOB_CONTROL         = 47, //< Job control.
    _SC_MAPPED_FILES        = 48, //< Memory mapped files.
    _SC_MEMLOCK             = 49, //< Process memory locking.
    _SC_MEMLOCK_RANGE       = 50, //< Range memory locking.
    _SC_MEMORY_PROTECTION   = 51, //< Process memory protection.
    _SC_MESSAGE_PASSING     = 52, //< Message passing.
    _SC_MONOTONIC_CLOCK     = 53, //< Monotonic clock.
    _SC_PRIORITIZED_IO      = 54, //< Prioritized input and output.
    _SC_PRIORITY_SCHEDULING = 55, //< Process scheduling.
    _SC_RAW_SOCKETS         = 56, //< Raw sockets.
    _SC_READER_WRITER_LOCKS = 57, //< Read-write locks.
    _SC_REALTIME_SIGNALS    = 58, //< Realtime signals.
    _SC_REGEXP              = 59, //< Regular expression handling.
    _SC_SAVED_IDS           = 60, //< Each process has a saved set-user-ID and a saved set-group-ID.
    _SC_SEMAPHORES          = 61, //< Semaphores.
    _SC_SHARED_MEMORY_OBJECTS      = 62, //< Shared Memory Objects.
    _SC_SHELL                      = 63, //< POSIX shell.
    _SC_SPAWN                      = 64, //< POSIX spawn.
    _SC_SPIN_LOCKS                 = 65, //< Spin Locks.
    _SC_SPORADIC_SERVER            = 66, //< Process Sporadic Server.
    _SC_SS_REPL_MAX                = 67, //< Maximum length of a stream head read or write queue.
    _SC_SYNCHRONIZED_IO            = 68, //< Synchronized I/O.
    _SC_THREAD_ATTR_STACKADDR      = 69, //< Thread Stack Address Attribute.
    _SC_THREAD_ATTR_STACKSIZE      = 70, //< Thread Stack Size Attribute.
    _SC_THREAD_CPUTIME             = 71, //< Thread CPU-Time Clock options.
    _SC_THREAD_PRIO_INHERIT        = 72, //< Non-Robust Mutex Priority Inheritance.
    _SC_THREAD_PRIO_PROTECT        = 73, //< Non-Robust Mutex Priority Protection.
    _SC_THREAD_PRIORITY_SCHEDULING = 74, //< Thread Execution Scheduling.
    _SC_THREAD_PROCESS_SHARED      = 75, //< Thread Process-Shared Synchronization.
    _SC_THREAD_ROBUST_PRIO_INHERIT = 76, //< Thread Robust Mutex Priority Inheritance.
    _SC_THREAD_ROBUST_PRIO_PROTECT = 77, //< Thread Robust Mutex Priority Protection.
    _SC_THREAD_SAFE_FUNCTIONS      = 78, //< Thread Safe Functions.
    _SC_THREAD_SPORADIC_SERVER     = 79, //< Thread Sporadic Server.
    _SC_THREADS                    = 80, //< Threads support.
    _SC_TIMEOUTS                   = 81, //< Timeouts for Threads.
    _SC_TIMERS                     = 82, //< Timers.
    _SC_TRACE                      = 83, //< Trace.
    _SC_TRACE_EVENT_FILTER         = 84, //< Trace Event Filter.
    _SC_TRACE_EVENT_NAME_MAX       = 85, //< Maximum length of trace event name.
    _SC_TRACE_INHERIT              = 86, //< Trace Inherit.
    _SC_TRACE_LOG                  = 87, //< Trace Log.
    _SC_TRACE_NAME_MAX             = 88, //< Maximum length of trace name.
    _SC_TRACE_SYS_MAX              = 89, //< Maximum number of trace events.
    _SC_TRACE_USER_EVENT_MAX       = 90, //< Maximum user trace events.
    _SC_TYPED_MEMORY_OBJECTS       = 91, //< Typed Memory Objects.
    _SC_VERSION = 92, //< Version of the POSIX specification with which the implementation complies.
    _SC_V7_ILP32_OFF32   = 93,  //< 32 bit int and 32 bit long, pointer and off_t.
    _SC_V7_ILP32_OFFBIG  = 94,  //< 32 bit int and 64 bit long, pointer and off_t.
    _SC_V7_LP64_OFF64    = 95,  //< 32 bit int and 64 bit long, pointer and off_t.
    _SC_V7_LPBIG_OFFBIG  = 96,  //< 64 bit int and 64 bit long, pointer and off_t.
    _SC_V6_ILP32_OFF32   = 97,  //< 32 bit int and 32 bit long, pointer and off_t.
    _SC_V6_ILP32_OFFBIG  = 98,  //< 32 bit int and 64 bit long, pointer and off_t.
    _SC_V6_LP64_OFF64    = 99,  //< 32 bit int and 64 bit long, pointer and off_t.
    _SC_V6_LPBIG_OFFBIG  = 100, //< 64 bit int and 64 bit long, pointer and off_t.
    _SC_2_C_BIND         = 101, //< C-Binding option.
    _SC_2_C_DEV          = 102, //< C-Language Development Utilities.
    _SC_2_CHAR_TERM      = 103, //< Terminal characteristics.
    _SC_2_FORT_DEV       = 104, //< Fortran Development Utilities.
    _SC_2_FORT_RUN       = 105, //< Fortran Runtime Utilities.
    _SC_2_LOCALEDEF      = 106, //< Localedef.
    _SC_2_PBS            = 107, //< Batch Environment Services.
    _SC_2_PBS_ACCOUNTING = 108, //< Batch Accounting Services.
    _SC_2_PBS_CHECKPOINT = 109, //< Batch Checkpoint/Restart.
    _SC_2_PBS_LOCATE     = 110, //< Locate Batch Job Request.
    _SC_2_PBS_MESSAGE    = 111, //< Batch Job Message Request.
    _SC_2_PBS_TRACK      = 112, //< Track Batch Job Request.
    _SC_2_SW_DEV         = 113, //< Software Development Utilities.
    _SC_2_UPE            = 114, //< User Portability Utilities.
    _SC_2_VERSION = 115, //< Version of POSIX specification with which the implementation complies.
    _SC_XOPEN_CRYPT            = 116, //< X/Open Encryption Option.
    _SC_XOPEN_ENH_I18N         = 117, //< X/Open Internationalization Option.
    _SC_XOPEN_REALTIME         = 118, //< X/Open Realtime.
    _SC_XOPEN_REALTIME_THREADS = 119, //< X/Open Realtime Threads.
    _SC_XOPEN_SHM              = 120, //< Shared Memory Open Group.
    _SC_XOPEN_STREAMS          = 121, //< X/Open STREAMS.
    _SC_XOPEN_UNIX             = 122, //< X/Open UNIX.
    _SC_XOPEN_UUCP             = 123, //< X/Open UUCP.
    _SC_XOPEN_VERSION          = 124, //< X/Open version.
    _SC_NPROCESSORS_ONLN       = 125, //< Number of processors currently online (available).
};

/// File stream constants
enum {
    STDIN_FILENO  = 0, //< Standard input file descriptor.
    STDOUT_FILENO = 1, //< Standard output file descriptor.
    STDERR_FILENO = 2  //< Standard error file descriptor.
};

/// Character disabling special character processing
#define _POSIX_VDISABLE 0xff

#ifdef __STDC_HOSTED__
extern char *optarg;               //< Argument to an option.
extern int opterr, optind, optopt; //< Option processing variables.

    #if defined(__cplusplus)
extern "C" {
    #endif

int access(const char *, int);
unsigned alarm(unsigned);
int chdir(const char *);
int chown(const char *, uid_t, gid_t);
int close(int);
size_t confstr(int, char *, size_t);

int dup(int);

int dup2(int, int);
int execl(const char *, const char *, ...);
int execle(const char *, const char *, ...);
int execlp(const char *, const char *, ...);
int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
void _exit(int);
int fchown(int, uid_t, gid_t);

char *crypt(const char *, const char *);
char *ctermid(char *);
void encrypt(char[64], int);
int fchdir(int);
int fdatasync(int);
int fsync(int);
long gethostid(void);
pid_t getpgid(pid_t);
pid_t getsid(pid_t);
char *getwd(char *);
int lchown(const char *, uid_t, gid_t);
int lockf(int, int, off_t);
int nice(int);
ssize_t pread(int, void *, size_t, off_t);
ssize_t pwrite(int, const void *, size_t, off_t);
pid_t setpgrp(void);
int setregid(gid_t, gid_t);
int setreuid(uid_t, uid_t);
void swab(const void *_RESTRICT, void *_RESTRICT, ssize_t);
void sync(void);
int truncate(const char *, off_t);
useconds_t ualarm(useconds_t, useconds_t);
int usleep(useconds_t);
pid_t vfork(void);

/**
 * @brief Create a new process (fork).
 *
 * The `fork` function creates a new process (child process) that is an exact copy of
 * the calling process (parent process). The child process starts executing from the
 * point of the `fork` call, and both the parent and child processes continue executing
 * independently from that point.
 *
 * The `fork` function returns the process ID (PID) of the child process to the parent
 * process, and 0 to the child process. If the `fork` call fails, it returns -1 to the
 * parent process, and no child process is created.
 *
 * @note The use of `fork` followed by `exec*` is discouraged for new software. Because of the use
 * of microkernel and the general system architectures, the `fork` operation can be relatively
 * inefficient and clunky. If possible, consider using `posix_spawn()` or other process creation
 * mechanisms that are more modern, efficient, flexible and better adjusted to the architecture.
 *
 * @return The PID of the child process to the parent process, 0 to the child process, or
 *         -1 on failure.
 */
pid_t fork(void);

long fpathconf(int, int);
int ftruncate(int, off_t);
char *getcwd(char *, size_t);
gid_t getegid(void);
uid_t geteuid(void);
gid_t getgid(void);
int getgroups(int, gid_t[]);
int gethostname(char *, size_t);
char *getlogin(void);
int getlogin_r(char *, size_t);
int getopt(int, char *const[], const char *);
pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
int isatty(int);
int link(const char *, const char *);

/**
 * @brief Set the file offset for a file descriptor.
 *
 * The `lseek` function sets the file offset for the file associated with the file descriptor `fd`
 * to the specified `offset` from the reference point `whence`. The reference point can be one of
 * the following constants:
 *
 * - `SEEK_SET`: The offset is relative to the beginning of the file.
 * - `SEEK_CUR`: The offset is relative to the current file position.
 * - `SEEK_END`: The offset is relative to the end of the file.
 *
 * After a successful call to `lseek`, the next operation on the file associated with `fd`,
 * such as a read or write, will occur at the specified position. If `lseek` encounters an error,
 * it will return -1, and the file offset may be unchanged.
 *
 * @param fd     The file descriptor of the file.
 * @param offset The offset value to set the file offset to.
 * @param whence The reference point from which to calculate the offset.
 * @return The resulting file offset from the beginning of the file, or -1 if an error occurred.
 */
off_t lseek(int fd, off_t offset, int whence);

long pathconf(const char *, int);
int pause(void);
int pipe(int[2]);
int pipe2(int[2], int);

/**
 * @brief Read data from a file descriptor.
 *
 * The `read` function reads up to `nbytes` bytes from the file descriptor `fd` into
 * the buffer pointed to by `buf`. The data read is stored in the buffer, and the
 * file offset associated with the file descriptor is incremented by the number of bytes
 * read. The function returns the number of bytes actually read, which may be less than
 * `nbytes` if there is not enough data available at the time of the call. A return value
 * of 0 indicates end-of-file (EOF) has been reached.
 *
 * If `read` encounters an error, it returns -1, and the contents of the buffer pointed
 * to by `buf` are undefined. The specific error can be retrieved using the `errno` variable.
 *
 * @param fd     The file descriptor from which to read data.
 * @param buf    Pointer to the buffer where the read data will be stored.
 * @param nbytes The maximum number of bytes to read.
 * @return The number of bytes actually read on success, 0 for end-of-file, or -1 on error.
 */
ssize_t read(int fd, void *buf, size_t nbytes);

ssize_t readlink(const char *, char *, size_t);
int rmdir(const char *);
int setegid(gid_t);
int seteuid(uid_t);
int setgid(gid_t);

int setpgid(pid_t, pid_t);
pid_t setsid(void);
int setuid(uid_t);

/**
 * @brief Sleep for a specified number of seconds.
 *
 * The `sleep` function suspends the execution of the calling thread for at least
 * the specified number of seconds, unless interrupted by a signal or an error occurs.
 *
 * @param seconds The number of seconds to sleep.
 * @return The number of seconds remaining to sleep (0 if the sleep completes).
 */
unsigned int sleep(unsigned int seconds);

int symlink(const char *, const char *);
long sysconf(int);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char *ttyname(int);
int ttyname_r(int, char *, size_t);
int unlink(const char *);
int unlinkat(int fd, const char *path, int flag);

/**
 * @brief Write data to a file descriptor.
 *
 * The write() function writes up to 'count' bytes from the buffer pointed to by
 * 'buffer' to the file descriptor 'fd'.
 *
 * @param fd      The file descriptor to write to.
 * @param buffer  Pointer to the data to be written.
 * @param count   The number of bytes to write.
 *
 * @return On success, the number of bytes written is returned. On error, -1 is
 * returned, and 'errno' is set to indicate the error.
 *
 * @note The write() function may not write all 'count' bytes if there is not
 * enough space available or if an error occurs. The caller should check the
 * return value to determine the number of bytes successfully written.
 */
ssize_t write(int, const void *, size_t);

/**
 * @brief Reads data from a file at a specified offset
 *
 * The pread() function reads 'count' bytes of data from the file associated with the file
 * descriptor 'fd' into the buffer pointed to by 'buf'. The read operation starts at the byte offset
 * 'offset' in the file. The file must have been opened with the read access mode.
 *
 * @param fd The file descriptor of the file to read from
 * @param buf The buffer to store the read data
 * @param count The number of bytes to read
 * @param offset The offset within the file to start reading from
 * @return On success, the number of bytes read is returned. On error, -1 is returned, and errno is
 * set appropriately. Possible error values:
 *         - EBADF: Invalid file descriptor 'fd'
 *         - EIO: I/O error occurred during the read operation
 *
 * @note This implementation assumes the availability of a filesystem daemon and uses an IPC
 * mechanism to communicate with it for performing the read operation.
 */
ssize_t pread(int fd, void *buf, size_t count, off_t offset);

/**
 * @brief Get the page size of the system.
 *
 * This function returns the system's page size in bytes. This function is equivalent to
 * sysconf(_SC_PAGESIZE) and is provided for compatibility because int might not be large enough to
 * represent the page size and cause problems on some future systems.
 *
 * @return The page size in bytes.
 * @see sysconf()
 */
int getpagesize(void);

    #if defined(__cplusplus)
} /* extern "C" */
    #endif

#endif // __STDC_HOSTED__

#endif // _UNISTD_H
