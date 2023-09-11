#ifndef __SIGNAL_H
#define __SIGNAL_H
#include <time.h>

#define __DECLARE_PID_T
#define __DECLARE_UID_T
#define __DECLARE_PTHREAD_T
#define __DECLARE_PTHREAD_ATTR_T
#define __DECLARE_STACK_T
#define __DECLARE_SIGSET_T
#include "__posix_types.h"

typedef int sig_atomic_t;

#define SIG_DFL ((void(*)(int))0)
#define SIG_ERR (void(*)(int)1)
#define SIG_IGN (void(*)(int)2)


#define SIGFPE 1
#define SIGILL 2
#define SIGINT 3
#define SIGSEGV 4
#define SIGTERM 5

// TODO
// #ifdef _POSIX_C_SOURCE

#define SIGALRM 6
#define SIGBUS 7
#define SIGCHLD 8
#define SIGCONT 9
#define SIGHUP 10
#define SIGKILL 11
#define SIGPIPE 12
#define SIGQUIT 13
#define SIGSTOP 14
#define SIGTSTP 15
#define SIGTTIN 16
#define SIGTTOU 17
#define SIGUSR1 18
#define SIGUSR2 19
#define SIGPOLL 20
#define SIGPROF 21
#define SIGSYS 22
#define SIGTRAP 23
#define SIGURG 24
#define SIGVTALRM 25
#define SIGXCPU 26
#define SIGXFSZ 27
#define SIGABRT 28

#define SIG_HOLD (void(*)(int)3)

#define SA_NOCLDSTOP 0
#define SIG_BLOCK 1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3
#define SA_ONSTACK 4
#define SA_RESETHAND 5
#define SA_RESTART 6
#define SA_SIGINFO 7
#define SA_NOCLDWAIT 8
#define SA_NODEFER 9
#define SS_ONSTACK 10
#define SS_DISABLE 11
#define MINSIGSTKSZ (2*4096)
#define SIGSTKSZ (32*4096)

#define SIGEV_NONE 0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

struct sigstack {
    int       ss_onstack;
    void     *ss_sp;
};

union sigval {
    int sival_int;
    void *sival_ptr;
};

#define SIGRTMIN 32
#define SIGRTMAX 64

#define NSIG 64

typedef struct {
    int           si_signo;
    int           si_code;
    int           si_errno;
    pid_t         si_pid;
    uid_t         si_uid;
    void         *si_addr;
    int           si_status;
    long          si_band;
    union sigval  si_value;
} siginfo_t;

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void(*sigev_notify_function)(union sigval);
    pthread_attr_t * sigev_notify_attributes;
};

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int      sa_flags;
    void (*sa_sigaction)(int, siginfo_t *, void *);
};

#define ILL_ILLOPC 0
#define ILL_ILLOPN 1
#define ILL_ILLADR 2
#define ILL_ILLTRP 3
#define ILL_PRVOPC 4
#define ILL_PRVREG 5
#define ILL_COPROC 6
#define ILL_BADSTK 7
#define FPE_INTDIV 8
#define FPE_INTOVF 9
#define FPE_FLTDIV 10
#define FPE_FLTOVF 11
#define FPE_FLTUND 12
#define FPE_FLTRES 13
#define FPE_FLTINV 14
#define FPE_FLTSUB 15
#define SEGV_MAPERR 16
#define SEGV_ACCERR 17
#define BUS_ADRALN 18
#define BUS_ADRERR 19
#define BUS_OBJERR 20
#define TRAP_BRKPT 21
#define TRAP_TRACE 22
#define CLD_EXITED 23
#define CLD_KILLED 24
#define CLD_DUMPED 25
#define CLD_TRAPPED 26
#define CLD_STOPPED 27
#define CLD_CONTINUED 28
#define POLL_IN 29
#define POLL_OUT 30
#define POLL_MSG 31
#define POLL_ERR 32
#define POLL_PRI 33
#define POLL_HUP 34
#define SI_USER 35
#define SI_QUEUE 36
#define SI_TIMER 37
#define SI_ASYNCIO 38
#define SI_MESGQ 39

#if defined(__cplusplus)
extern "C" {
#endif

void (*signal(int sig, void (*func)(int)))(int);
int raise(int sig);

void (*bsd_signal(int, void (*)(int)))(int);
int    kill(pid_t, int);
int    killpg(pid_t, int);
int    pthread_kill(pthread_t, int);
int    pthread_sigmask(int, const sigset_t *, sigset_t *);
int    raise(int);
int    sigaction(int, const struct sigaction *, struct sigaction *);
int    sigaddset(sigset_t *, int);
int    sigaltstack(const stack_t *, stack_t *);
int    sigdelset(sigset_t *, int);
int    sigemptyset(sigset_t *);
int    sigfillset(sigset_t *);
int    sighold(int);
int    sigignore(int);
int    siginterrupt(int, int);
int    sigismember(const sigset_t *, int);
void (*signal(int, void (*)(int)))(int);
int    sigpause(int);
int    sigpending(sigset_t *);
int    sigprocmask(int, const sigset_t *, sigset_t *);
int    sigqueue(pid_t, int, const union sigval);
int    sigrelse(int);
void (*sigset(int, void (*)(int)))(int);
int    sigsuspend(const sigset_t *);
int    sigtimedwait(const sigset_t *, siginfo_t *, const struct timespec *);
int    sigwait(const sigset_t *, int *);
int    sigwaitinfo(const sigset_t *, siginfo_t *);

#if defined(__cplusplus)
} /* extern "C" */
#endif

// #endif _POSIX_C_SOURCE


#endif // __SIGNAL_H