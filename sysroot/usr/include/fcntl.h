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
#define O_DSYNC    (020000)
#define O_RSYNC    (04010000)

#define O_ACCMODE (07)

#define O_SEARCH  (00)
#define O_RDONLY  (01)
#define O_RDWR    (02)
#define O_WRONLY  (03)

#define AT_FDCWD (-1)

#define AT_EACCESS (1)

#define AT_SYMLINK_NOFOLLOW (1)

#define AT_SYMLINK_FOLLOW   (1)

#define AT_REMOVEDIR (1)

#define POSIX_FADV_NORMAL     (0)
#define POSIX_FADV_RANDOM     (1)
#define POSIX_FADV_SEQUENTIAL (2)
#define POSIX_FADV_WILLNEED   (3)
#define POSIX_FADV_DONTNEED   (4)
#define POSIX_FADV_NOREUSE    (5)

struct flock {
    short l_type; //< Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK
    short l_whence; //< Where `l_start` is relative to (like `whence` in `lseek`)
    off_t l_start; //< Offset where the lock begins
    off_t l_len; //< Number of bytes to lock. 0 means "until EOF".
    pid_t l_pid; //< Process holding the lock. F_GETLK fills this field on return.
};


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