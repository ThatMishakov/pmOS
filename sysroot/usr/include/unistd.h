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

long         pathconf(const char *, int);
int          pause(void);
int          pipe(int [2]);

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

/**
 * @brief Reads data from a file at a specified offset
 *
 * The pread() function reads 'count' bytes of data from the file associated with the file descriptor 'fd'
 * into the buffer pointed to by 'buf'. The read operation starts at the byte offset 'offset' in the file.
 * The file must have been opened with the read access mode.
 *
 * @param fd The file descriptor of the file to read from
 * @param buf The buffer to store the read data
 * @param count The number of bytes to read
 * @param offset The offset within the file to start reading from
 * @return On success, the number of bytes read is returned. On error, -1 is returned, and errno is set appropriately.
 *         Possible error values:
 *         - EBADF: Invalid file descriptor 'fd'
 *         - EIO: I/O error occurred during the read operation
 *
 * @note This implementation assumes the availability of a filesystem daemon and uses an IPC mechanism to communicate
 *       with it for performing the read operation.
 */
ssize_t pread(int fd, void *buf, size_t count, off_t offset);

int getpagesize(void);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

