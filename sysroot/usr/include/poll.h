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

#ifndef _POLL_H
#define _POLL_H

struct pollfd {
    int fd; //< file descriptor
    short events; //< requested events
    short revents; //< returned events
};

typedef unsigned long nfds_t;

/// Constants for the 'events' and 'revents' field of struct pollfd
enum {
    POLLIN = 0x001, //< There is data to read
    POLLOUT = 0x002, //< Writing now will not block
    POLLRDNORM = 0x004, //< Normal data may be read
    POLLRDBAND = 0x008, //< Priority data may be read
    POLLPRI = 0x010, //< Priority data may be read
    POLLWRNORM = POLLOUT, //< Writing now will not block
    POLLWRBAND = 0x020, //< Priority data may be written
    POLLNVAL = 0x040, //< Invalid request: fd not open
    POLLHUP = 0x2000, //< Hang up
    POLLERR = 0x4000, //< Error
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _POLL_H