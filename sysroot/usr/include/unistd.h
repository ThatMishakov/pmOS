#ifndef _UNISTD_H
#define _UNISTD_H 1

#if defined(__cplusplus)
extern "C" {
#endif


#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L
#define _XOPEN_VERSION 700

#define _POSIX_MESSAGE_PASSING 200809L
#define _POSIX_RAW_SOCKETS 200809L
#define _POSIX_SHARED_MEMORY_OBJECTS 200809L

typedef unsigned long int pid_t;
typedef long int intptr_t;
typedef unsigned long long int uintprt_t;

#ifdef __STDC_HOSTED__

pid_t fork(void);
int execv(const char *, char *const []);
int execve(const char *, char *const [], char *const []);
int execvp(const char *, char *const []);
pid_t getpid(void);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

