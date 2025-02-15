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

#ifndef FILESYSTEM_ADAPTORS_H
#define FILESYSTEM_ADAPTORS_H

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

typedef ssize_t(read_func)(void *file_data, uint64_t consumer_id, void *buf, size_t count,
                           size_t offset);
typedef ssize_t(write_func)(void *file_data, uint64_t consumer_id, const void *buf, size_t count,
                            size_t offset);
typedef int(clone_func)(void *file_data, uint64_t consumer_id, void *new_data,
                        uint64_t new_consumer_id);
typedef int(close_func)(void *file_data, uint64_t consumer_id);
typedef int(fstat_func)(void *file_data, uint64_t consumer_id, struct stat *statbuf);
typedef int(isatty_func)(void *file_data, uint64_t consumer_id);
typedef int(isseekable_func)(void *file_data, uint64_t consumer_id);
typedef ssize_t(filesize_func)(void *file_data, uint64_t consumer_id);

typedef ssize_t(writev_func)(void *file_data, uint64_t consumer_id, const struct iovec *iov,
                             int iovcnt, size_t offset);

/// @brief Function to free the file data.
///
/// This function must free all the memory associated with the given file data.
typedef void(free_func)(void *file_data, uint64_t consumer_id);

/**
 * @brief Adaptors for different types of file descriptors.
 *
 * This is used to allow the same code to be used for different types of file descriptors. Since
 * pmOS is based on a microkernel, the filesystem is implemented in userspace, with different
 * daemons implementing different file types, in particular vfsd is responsible for the virtual
 * filesystem, loggerd is responsible for the logging system, pipesd is responsible for pipes, etc.
 * Each daemon implements its own file descriptor type, with it's acompanying IPC interface, with
 * clients implemented in libc. This structure is a universal interface for all of these types of
 * file descriptors.
 */
struct Filesystem_Adaptor {
    read_func *read;
    write_func *write;
    writev_func *writev;
    clone_func *clone;
    close_func *close;
    fstat_func *fstat;
    isatty_func *isatty;
    isseekable_func *isseekable;
    filesize_func *filesize;
    free_func *free;
};

#endif // FILESYSTEM_ADAPTORS_H