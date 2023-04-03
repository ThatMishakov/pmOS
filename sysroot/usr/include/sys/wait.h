#ifndef __SYS_WAIT_H
#define __SYS_WAIT_H
#include "../signal.h"
#include "resource.h"

#define WNOHANG 0
#define WUNTRACED 1

#define WEXITSTATUS 0
#define WIFCONTINUED 1
#define WIFEXITED 2
#define WIFSIGNALED 3
#define WIFSTOPPED 4
#define WSTOPSIG 5
#define WTERMSIG 6

#define WEXITED 0
#define WSTOPPED 1
#define WCONTINUED 2
#define WNOHANG 3
#define WNOWAIT 4

#define __DECLARE_IDTYPE_T
#define __DECLARE_ID_T
#define __DECLARE_PID_T
#include "../__posix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif
#ifdef __STDC_HOSTED__

pid_t  wait(int *);
int    waitid(idtype_t, id_t, siginfo_t *, int);
pid_t  waitpid(pid_t, int *, int);


#endif /* __STDC_HOSTED__ */

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif