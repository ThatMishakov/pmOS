#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <sys/types.h>

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

int          access(const char *, int);
unsigned     alarm(unsigned);
int          chdir(const char *);
int          chown(const char *, uid_t, gid_t);
int          close(int);
size_t       confstr(int, char *, size_t);


int          dup(int);


int          dup2(int, int);
int          execl(const char *, const char *, ...);
int          execle(const char *, const char *, ...);
int          execlp(const char *, const char *, ...);
int          execv(const char *, char *const []);
int          execve(const char *, char *const [], char *const []);
int          execvp(const char *, char *const []);
void        _exit(int);
int          fchown(int, uid_t, gid_t);
pid_t        fork(void);
long         fpathconf(int, int);
int          ftruncate(int, off_t);
char        *getcwd(char *, size_t);
gid_t        getegid(void);
uid_t        geteuid(void);
gid_t        getgid(void);
int          getgroups(int, gid_t []);
int          gethostname(char *, size_t);
char        *getlogin(void);
int          getlogin_r(char *, size_t);
int          getopt(int, char * const [], const char *);
pid_t        getpgrp(void);
pid_t        getpid(void);
pid_t        getppid(void);
uid_t        getuid(void);
int          isatty(int);
int          link(const char *, const char *);
off_t        lseek(int, off_t, int);
long         pathconf(const char *, int);
int          pause(void);
int          pipe(int [2]);
ssize_t      read(int, void *, size_t);
ssize_t      readlink(const char *, char *, size_t);
int          rmdir(const char *);
int          setegid(gid_t);
int          seteuid(uid_t);
int          setgid(gid_t);


int          setpgid(pid_t, pid_t);
pid_t        setsid(void);
int          setuid(uid_t);
unsigned     sleep(unsigned);
int          symlink(const char *, const char *);
long         sysconf(int);
pid_t        tcgetpgrp(int);
int          tcsetpgrp(int, pid_t);
char        *ttyname(int);
int          ttyname_r(int, char *, size_t);
int          unlink(const char *);
ssize_t      write(int, const void *, size_t);

int getpagesize(void);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

