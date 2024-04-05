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

#include <pthread.h>
#include <pmos/ports.h>
#include <pmos/tls.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include <pmos/helpers.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

void __release_tls(struct uthread * u);

__thread pmos_port_t join_notify_port = INVALID_PORT;

static int prepare_join_notify_port()
{
    ports_request_t new_port = create_port(PID_SELF, 0);
    if (new_port.result != SUCCESS)
        return -1;

    join_notify_port = new_port.port;
    return 0;
}



int pthread_join(pthread_t thread, void ** retval)
{
    if (thread == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct uthread * u = thread;
    if (u->thread_status == __THREAD_STATUS_DETACHED) {
        errno = EINVAL;
        return -1;
    }

    if (u->thread_status == __THREAD_STATUS_FINISHED) {
        if (retval != NULL)
            *retval = u->return_value;
        __release_tls(u);
        return 0;
    }

    if (join_notify_port == INVALID_PORT) {
        if (prepare_join_notify_port() != 0)
            return -1;
    }

    u->join_notify_port = join_notify_port;

    bool is_running = __sync_bool_compare_and_swap(&u->thread_status, __THREAD_STATUS_RUNNING, __THREAD_STATUS_JOINING);
    if (!is_running) {
        // The thread is already finished
        assert(u->thread_status == __THREAD_STATUS_FINISHED);
        if (retval != NULL)
            *retval = u->return_value;
        return 0;
    }

    // Wait for the thread to finish

    Message_Descriptor reply_desc;
    IPC_Thread_Finished * reply;
    result_t result = get_message(&reply_desc, (unsigned char **)&reply, join_notify_port);
    if (result != SUCCESS) {
        errno = ESRCH;
        return -1;
    }

    if (reply_desc.size < sizeof(IPC_Thread_Finished)) {
        errno = ESRCH;
        return -1;
    }

    if (reply->type != IPC_Thread_Finished_NUM) {
        errno = ESRCH;
        return -1;
    }

    if (reply->thread_id != (uint64_t)thread) {
        errno = ESRCH;
        return -1;
    }

    // The thread has finished

    if (retval != NULL)
        *retval = u->return_value;

    return 0;
}