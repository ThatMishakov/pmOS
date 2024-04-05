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

#ifndef _SYS_UIO_H
#define _SYS_UIO_H 1

#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#include "__posix_types.h"

struct iovec {
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