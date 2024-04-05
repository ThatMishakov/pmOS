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

#include <pmos/tls.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

void __release_tls(struct uthread * u);

int pthread_detach(pthread_t thread)
{
    struct uthread * u = thread;
    if (u == NULL) {
        errno = EINVAL;
        return -1;
    }   

    if (u->thread_status == __THREAD_STATUS_DETACHED || u->thread_status == __THREAD_STATUS_JOINING) {
        errno = EINVAL;
        return -1;
    }

    // I don't think it makes a big difference but this potentially saves compare and swap, which in theory is expensive
    if (u->thread_status == __THREAD_STATUS_FINISHED) {
        __release_tls(u);
        return 0;
    }

    bool is_running = __sync_bool_compare_and_swap(&u->thread_status, __THREAD_STATUS_RUNNING, __THREAD_STATUS_DETACHED);
    if (!is_running) {
        // The thread is already finished (or is joining, which POSIX does not allow)
        assert(u->thread_status == __THREAD_STATUS_FINISHED);
        __release_tls(u);
    }

    return 0;
}