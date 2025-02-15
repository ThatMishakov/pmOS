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

#define SIGFPE  1
#define SIGILL  2
#define SIGINT  3
#define SIGSEGV 4
#define SIGTERM 5

// TODO
// #ifdef _POSIX_C_SOURCE

#define SIGALRM   6
#define SIGBUS    7
#define SIGCHLD   8
#define SIGCONT   9
#define SIGHUP    10
#define SIGKILL   11
#define SIGPIPE   12
#define SIGQUIT   13
#define SIGSTOP   14
#define SIGTSTP   15
#define SIGTTIN   16
#define SIGTTOU   17
#define SIGUSR1   18
#define SIGUSR2   19
#define SIGPOLL   20
#define SIGIO     20
#define SIGPROF   21
#define SIGSYS    22
#define SIGTRAP   23
#define SIGURG    24
#define SIGVTALRM 25
#define SIGXCPU   26
#define SIGXFSZ   27
#define SIGABRT   28
#define SIGIOT    28
#define SIGWINCH  29
#define SIGPWR    30
#define SIGINFO   31

#define SIG_HOLD (void (*)(int) 3)

#define SIG_BLOCK    1
#define SIG_UNBLOCK  2
#define SIG_SETMASK  3

#define SA_NOCLDSTOP 0x00000001
#define SA_ONSTACK   0x00000002
#define SA_RESETHAND 0x00000004
#define SA_RESTART   0x00000008
#define SA_SIGINFO   0x00000010
#define SA_NOCLDWAIT 0x00000020
#define SA_NODEFER   0x00000040
#define SS_ONSTACK   1
#define SS_DISABLE   2
#define MINSIGSTKSZ  (4 * 4096)
#define SIGSTKSZ     (32 * 4096)

#define SIGEV_NONE   0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

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

#define SIGRTMIN 32
#define SIGRTMAX 64

#define NSIG 64

typedef struct {
    int si_signo;
    int si_code;
    int si_errno;
    pid_t si_pid;
    uid_t si_uid;
    void *si_addr;
    int si_status;
    long si_band;
    union sigval si_value;
} siginfo_t;

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
    };
    sigset_t sa_mask;
    int sa_flags;
};

#define ILL_ILLOPC    0
#define ILL_ILLOPN    1
#define ILL_ILLADR    2
#define ILL_ILLTRP    3
#define ILL_PRVOPC    4
#define ILL_PRVREG    5
#define ILL_COPROC    6
#define ILL_BADSTK    7
#define FPE_INTDIV    8
#define FPE_INTOVF    9
#define FPE_FLTDIV    10
#define FPE_FLTOVF    11
#define FPE_FLTUND    12
#define FPE_FLTRES    13
#define FPE_FLTINV    14
#define FPE_FLTSUB    15
#define SEGV_MAPERR   16
#define SEGV_ACCERR   17
#define BUS_ADRALN    18
#define BUS_ADRERR    19
#define BUS_OBJERR    20
#define TRAP_BRKPT    21
#define TRAP_TRACE    22
#define CLD_EXITED    23
#define CLD_KILLED    24
#define CLD_DUMPED    25
#define CLD_TRAPPED   26
#define CLD_STOPPED   27
#define CLD_CONTINUED 28
#define POLL_IN       29
#define POLL_OUT      30
#define POLL_MSG      31
#define POLL_ERR      32
#define POLL_PRI      33
#define POLL_HUP      34
#define SI_USER       35
#define SI_QUEUE      36
#define SI_TIMER      37
#define SI_ASYNCIO    38
#define SI_MESGQ      39

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