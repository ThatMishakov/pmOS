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

#include <assert.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// int pmos_request_timer(pmos_port_t port, size_t ms) {
//     if (sleep_reply_port == INVALID_PORT) {
//         ports_request_t port_request = create_port(TASK_ID_SELF, 0);
//         if (port_request.result != SUCCESS) {
//             errno = port_request.result;
//             return -1;
//         }
//         sleep_reply_port = port_request.port;
//     }

//     IPC_Start_Timer tmr = {
//         .type = IPC_Start_Timer_NUM,
//         .ms = ms,
//         .reply_port = sleep_reply_port,
//     };

//     pmos_port_t sleep_port = get_sleep_port();
//     if (sleep_port == INVALID_PORT) {
//         // Errno is set by get_sleep_port
//         // errno = ENOSYS;
//         return -1;
//     }

//     result_t result = send_message_port(sleep_port, sizeof(tmr), (char*)&tmr);
//     if (result != SUCCESS) {
//         errno = result;
//         return -1;
//     }

//     return 0;
// }

result_t __pmos_request_timer(pmos_port_t port, uint64_t ns, uint64_t extra);

int pmos_request_timer(pmos_port_t port, uint64_t ns, uint64_t extra)
{
    // Request a timer from the kernel
    // Initially, this was implemented using HPET in the userspace, on x86.
    // However, other arches (RISC-V) don't have a global timer and even on x86,
    // it might be better to do this in kernel and use LAPIC timer, freeing HPET
    // for other

    result_t r = __pmos_request_timer(port, ns, extra);
    if (r != SUCCESS) {
        errno = -r;
        return -1;
    }

    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    struct timespec req = {.tv_sec = seconds, .tv_nsec = 0};
    struct timespec rem = {.tv_sec = 0, .tv_nsec = 0};
    int ret            = nanosleep(&req, &rem);
    if (ret == -1) {
        return rem.tv_sec + (rem.tv_nsec > 0);
    }
    return 0;
}