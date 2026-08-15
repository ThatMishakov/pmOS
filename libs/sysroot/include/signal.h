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

#ifndef _SIGNAL_H
#define _SIGNAL_H

#define __DECLARE_PID_T
#define __DECLARE_UID_T
#define __DECLARE_PTHREAD_T
#define __DECLARE_PTHREAD_ATTR_T
#define __DECLARE_STACK_T
#define __DECLARE_SIGSET_T
#define __DECLARE_TIMESPEC_T
#include "__posix_types.h"

typedef unsigned long sig_atomic_t;

#define SIG_DFL ((void (*)(int))0)
#define SIG_ERR ((void (*)(int))1)
#define SIG_IGN ((void (*)(int))2)

#define SIGABRT 6
#define SIGFPE  8
#define SIGILL  4
#define SIGINT  2
#define SIGSEGV 11
#define SIGTERM 15
#define SIGPROF 27
#define SIGIO   29
#define SIGPWR  30
#define SIGRTMIN 35
#define SIGRTMAX 64

// TODO
// #ifdef _POSIX_C_SOURCE

#define SIGHUP    1
#define SIGQUIT   3
#define SIGTRAP   5
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGWINCH  28
#define SIGPOLL   29
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define SIGCANCEL 32
#define SIGTIMER  33

#define SIG_HOLD (void (*)(int) 3)

#define SIG_BLOCK    0
#define SIG_UNBLOCK  1
#define SIG_SETMASK  2

#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO   4
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000
#define SA_RESTORER  0x04000000
#define SS_ONSTACK   1
#define SS_DISABLE   2
#define MINSIGSTKSZ  (4 * 4096)
#define SIGSTKSZ     (32 * 4096)

#define SIGEV_NONE   1
#define SIGEV_SIGNAL 0
#define SIGEV_THREAD 2
#define SIGEV_THREAD_ID 4

extern const char *const sys_siglist[];
extern const char *const sys_signame[];

struct sigstack {
    int ss_onstack;
    void *ss_sp;
};

union sigval {
    int sival_int;
    void *sival_ptr;
};

#define SIGRTMIN 35
#define SIGRTMAX 64

#define _NSIG 65
#define NSIG _NSIG

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    union {
        char __pad[128 - 2 * sizeof(int) - sizeof(long)];
        struct {
            union {
                struct {
                    pid_t si_pid;
                    uid_t si_uid;
                } __piduid;
                struct {
                    int si_timerid;
                    int si_overrun;
                } __timer;
            } __first;
            union {
                union sigval si_value;
                struct {
                    int si_status;
                    long si_utime;
                    long si_stime;
                } __sigchld;
            } __second;
        } __si_common;
        struct {
            void *si_addr;
            short si_addr_lsb;
            union {
                struct {
                    void *si_lower;
                    void *si_upper;
                } __addr_bnd;
                unsigned si_pkey;
            } __first;
        } __sigfault;
        struct {
            long si_band;
            int si_fd;
        } __sigpoll;
        struct {
            void *si_call_addr;
            int si_syscall;
            unsigned si_arch;
        } __sigsys;
    } __si_fields;
} siginfo_t;

#define si_pid     __si_fields.__si_common.__first.__piduid.si_pid
#define si_uid     __si_fields.__si_common.__first.__piduid.si_uid
#define si_status  __si_fields.__si_common.__second.__sigchld.si_status
#define si_utime   __si_fields.__si_common.__second.__sigchld.si_utime
#define si_stime   __si_fields.__si_common.__second.__sigchld.si_stime
#define si_value   __si_fields.__si_common.__second.si_value
#define si_addr    __si_fields.__sigfault.si_addr
#define si_addr_lsb __si_fields.__sigfault.si_addr_lsb
#define si_lower   __si_fields.__sigfault.__first.__addr_bnd.si_lower
#define si_upper   __si_fields.__sigfault.__first.__addr_bnd.si_upper
#define si_pkey    __si_fields.__sigfault.__first.si_pkey
#define si_band    __si_fields.__sigpoll.si_band
#define si_fd      __si_fields.__sigpoll.si_fd
#define si_timerid __si_fields.__si_common.__first.__timer.si_timerid
#define si_overrun __si_fields.__si_common.__first.__timer.si_overrun
#define si_ptr     si_value.sival_ptr
#define si_int     si_value.sival_int
#define si_call_addr __si_fields.__sigsys.si_call_addr
#define si_syscall __si_fields.__sigsys.si_syscall
#define si_arch    __si_fields.__sigsys.si_arch

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    pthread_attr_t *sigev_notify_attributes;
};

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } __sa_handler;
    int sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
};

#define sa_handler __sa_handler.sa_handler
#define sa_sigaction __sa_handler.sa_sigaction

#define ILL_ILLOPC    1
#define ILL_ILLOPN    2
#define ILL_ILLADR    3
#define ILL_ILLTRP    4
#define ILL_PRVOPC    5
#define ILL_PRVREG    6
#define ILL_COPROC    7
#define ILL_BADSTK    8
#define ILL_BADIADDR  9
#define FPE_INTDIV    1
#define FPE_INTOVF    2
#define FPE_FLTDIV    3
#define FPE_FLTOVF    4
#define FPE_FLTUND    5
#define FPE_FLTRES    6
#define FPE_FLTINV    7
#define FPE_FLTSUB    8
#define SEGV_MAPERR   1
#define SEGV_ACCERR   2
#define BUS_ADRALN    1
#define BUS_ADRERR    2
#define BUS_OBJERR    3
#define BUS_MCEERR_AR 4
#define BUS_MCEERR_AO 5
#define TRAP_BRKPT    1
#define TRAP_TRACE    2
#define CLD_EXITED    1
#define CLD_KILLED    2
#define CLD_DUMPED    3
#define CLD_TRAPPED   4
#define CLD_STOPPED   5
#define CLD_CONTINUED 6
#define POLL_IN       1
#define POLL_OUT      2
#define POLL_MSG      3
#define POLL_ERR      4
#define POLL_PRI      5
#define POLL_HUP      6
#define SI_ASYNCNL    (-60)
#define SI_TKILL      (-6)
#define SI_SIGIO      (-5)
#define SI_ASYNCIO    (-4)
#define SI_MESGQ      (-3)
#define SI_TIMER      (-2)
#define SI_QUEUE      (-1)
#define SI_USER       0
#define SI_KERNEL     128

#if defined(__cplusplus)
extern "C" {
#endif

void (*signal(int sig, void (*func)(int)))(int);
int raise(int sig);

void (*bsd_signal(int, void (*)(int)))(int);
int kill(pid_t, int);
int killpg(pid_t, int);
int pthread_kill(pthread_t, int);
int pthread_sigmask(int, const sigset_t * _RESTRICT, sigset_t * _RESTRICT);
int raise(int);
int sigaction(int, const struct sigaction *, struct sigaction *);
int sigaddset(sigset_t *, int);
int sigaltstack(const stack_t *, stack_t *);
int sigdelset(sigset_t *, int);
int sigemptyset(sigset_t *);
int sigfillset(sigset_t *);
int sighold(int);
int sigignore(int);
int siginterrupt(int, int);
int sigismember(const sigset_t *, int);
void (*signal(int, void (*)(int)))(int);
int sigpause(int);
int sigpending(sigset_t *);
int sigprocmask(int, const sigset_t * _RESTRICT, sigset_t * _RESTRICT);
int sigqueue(pid_t, int, const union sigval);
int sigrelse(int);
void (*sigset(int, void (*)(int)))(int);
int sigsuspend(const sigset_t *);
int sigtimedwait(const sigset_t *, siginfo_t *, const struct timespec *);
int sigwait(const sigset_t *, int *);
int sigwaitinfo(const sigset_t *, siginfo_t *);

void psignal(int sig, const char *s);

#if defined(__cplusplus)
} /* extern "C" */
#endif

// #endif _POSIX_C_SOURCE

#endif // _SIGNAL_H