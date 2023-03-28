#ifndef __FCNTL_H
#define __FCNTL_H
#include <sys/types.h>

#define F_DUPFD (0)
#define F_GETFD (1)
#define F_SETFD (2)
#define F_GETFL (3)
#define F_SETFL (4)
#define F_GETLK (5)
#define F_SETLK (6)
#define F_SETLKW (7)
#define F_GETOWN (8)
#define F_SETOWN (9)

#define FD_CLOEXEC (0)

#define F_RDLCK (0)
#define F_UNLCK (1)
#define F_WRLCK (2)

#define O_CREAT  (04)
#define O_EXCL   (010)
#define O_NOCTTY (020)
#define O_TRUNC  (040)
#define O_CLOEXEC (0100)
#define O_DIRECTORY (0200)
#define O_NOFOLLOW  (0400)
#define O_TTY_INIT  (01000)

#define O_APPEND   (02000)
#define O_NONBLOCK (04000)
#define O_SYNC     (010000)

#define O_ACCMODE (1)

#define O_SEARCH  (00)
#define O_RDONLY  (01)
#define O_RDWR    (02)
#define O_WRONLY  (03)

#if defined(__cplusplus)
extern "C" {
#endif

int  creat(const char *, mode_t);
int  fcntl(int, int, ...);
int  open(const char * path, int oflag, ...);

int  openat(int, const char *, int, ...);

int  posix_fadvise(int, off_t, off_t, int);
int  posix_fallocate(int, off_t, off_t);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif