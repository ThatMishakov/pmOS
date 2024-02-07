#ifndef FILESYSTEM_ADAPTORS_H
#define FILESYSTEM_ADAPTORS_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef ssize_t (read_func) (void * file_data, uint64_t consumer_id, void * buf, size_t count, size_t offset);
typedef ssize_t (write_func) (void * file_data, uint64_t consumer_id, const void * buf, size_t count, size_t offset);
typedef int (clone_func) (void * file_data, uint64_t consumer_id, void * new_data, uint64_t new_consumer_id);
typedef int (close_func) (void * file_data, uint64_t consumer_id);
typedef int (fstat_func) (void * file_data, uint64_t consumer_id, struct stat * statbuf);
typedef int (isatty_func) (void * file_data, uint64_t consumer_id);
typedef int (isseekable_func) (void * file_data, uint64_t consumer_id);
typedef ssize_t (filesize_func) (void * file_data, uint64_t consumer_id);

/// @brief Function to free the file data.
///
/// This function must free all the memory associated with the given file data.
typedef void (free_func) (void * file_data, uint64_t consumer_id);

/**
 * @brief Adaptors for different types of file descriptors.
 * 
 * This is used to allow the same code to be used for different types of file descriptors. Since pmOS is based
 * on a microkernel, the filesystem is implemented in userspace, with different daemons implementing different
 * file types, in particular vfsd is responsible for the virtual filesystem, loggerd is responsible for the
 * logging system, pipesd is responsible for pipes, etc. Each daemon implements its own file descriptor type,
 * with it's acompanying IPC interface, with clients implemented in libc. This structure is a universal
 * interface for all of these types of file descriptors.
 */
struct Filesystem_Adaptor {
    read_func * read;
    write_func * write;
    clone_func * clone;
    close_func * close;
    fstat_func * fstat;
    isatty_func * isatty;
    isseekable_func * isseekable;
    filesize_func * filesize;
    free_func * free;
};

#endif // FILESYSTEM_ADAPTORS_H