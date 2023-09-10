#ifndef _SYS_UIO_H
#define _SYS_UIO_H 1

#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#include "__posix_types.h"

struct io_vec {
    void *iov_base; //< Base address of the buffer.
    size_t iov_len; //< Size of the buffer.
};

#ifdef __STDC_HOSTED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read data from a file descriptor into a buffer.
 *
 * The `read` function reads up to `count` bytes from the file descriptor `fd` into
 * the buffer starting at `buf`.
 *
 * @param fd    The file descriptor to read from.
 * @param buf   The buffer to read into.
 * @param count The maximum number of bytes to read.
 * @return      The number of bytes read, or `-1` if an error occurred.
 *
 * @note        The `read` function is a cancellation point.
 */
ssize_t readv(int fd, const struct iovec * buf, size_t count);

/**
 * @brief Write data from a buffer to a file descriptor.
 *
 * The `write` function writes up to `count` bytes from the buffer starting at `buf`
 * to the file descriptor `fd`.
 *
 * @param fd    The file descriptor to write to.
 * @param buf   The buffer to write from.
 * @param count The maximum number of bytes to write.
 * @return      The number of bytes written, or `-1` if an error occurred.
 *
 * @note        The `write` function is a cancellation point.
 */
ssize_t writev(int fd, const struct iovec * buf, size_t count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

#endif