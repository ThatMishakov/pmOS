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

#ifndef _PMOS_HELPERS_H
#define _PMOS_HELPERS_H

#include <kernel/messaging.h>
#include "system.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint64_t pmos_port_t;

#ifdef __STDC_HOSTED__

/**
 * @brief Blocks the process and upon the message reception, allocates memory with malloc(), and fills it with the content. Internally,
 *        calls get_message_info() and get_first_message()
 * 
 * @param desc Pointer to the memory location where Message_descriptor will be filled
 * @param message malloc'ed message. To avoid memory leaks, it should be free'd after no longer nedded
 * @param port Valid port, to which the callee should be the owner, from where to get the message
 * @return result of the execution
 */
result_t get_message(Message_Descriptor* desc, unsigned char** message, pmos_port_t port);

/**
 * @brief Requests a timer message from the system. On success, the given port will receive a IPC_Timer_Reply
 *        after at least ms milliseconds.
 * 
 * @param port Port to receive the message
 * @param ms Time in milliseconds
 * @return int 0 on success, -1 on failure. Sets errno on error
*/
int pmos_request_timer(pmos_port_t port, size_t ms);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif