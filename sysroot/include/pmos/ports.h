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

#ifndef _PMOS_PORTS_H
#define _PMOS_PORTS_H
#include "../kernel/syscalls.h"
#include "../kernel/types.h"
#include "system.h"

#include <stdint.h>
#include <unistd.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef TASK_ID_SELF
    #define TASK_ID_SELF 0
#endif

typedef uint64_t pmos_port_t;

typedef struct ports_request_t {
    result_t result;
    pmos_port_t port;
} ports_request_t;

#ifdef __STDC_HOSTED__

/**
 * @brief Create a new IPC port.
 *
 * This syscall created a new messaging port for the requesting owner. The port holds an infinite
 * queue of messages. Any process which has necessary rights can send messages to the port, however
 * only its owner may get the messages contained in it. It is planned to destroy the ports
 * automatically if the owner gets destroyed, however in current implementation it is not the case
 * and the ports do not get destroyed automatically.
 *
 * @param owner The owner of the new port. Takes TASK_ID_SELF
 * @param flags Currently unused. Must be set to 0.
 * @return ports_request_t the result of the operation and the id of the new port. If the operation
 * was not successfull, *port* does not hols a meningful value.
 * @todo Make the tasks own the ports and destroy them automatically when the task is destroyed.
 * Consider ports with queues other than FIFO as that sounds very interesting
 */
ports_request_t create_port(pid_t owner, uint32_t flags);

result_t pmos_delete_port(pmos_port_t port);

/**
 * @brief Assigns a name to the right.
 *
 * This syscall assigns a name to the IPC right. Internally, it creates a Named_Port object, which
 * points to a port. If the named port already exists, this replaces it. The tasks
 * might then reference this name to get the underlying IPC right. In addition, during the creation
 * of the object, if there are waiting tasks, they immediately get unblocked and recieve the new
 * right number. See get_right_by_name(). The right must be send-many.
 *
 * @param right_id The ID of the IPC right to which the name would be asigned, within the caller
 *                 thread rights namespace.
 * @param name The pointer to the string holding the right name.
 * @param length The length of the name. In current implementation, there is no limit.
 * @param flags Currently unused. Must be set to 0.
 * @return result_t Result of the operation.
 * @see get_right_by_name()
 */
result_t name_right(pmos_port_t right_id, const char *name, size_t length, uint32_t flags);

/**
 * @brief Get the port by name object.
 *
 * This function gets a right referenced by the *name* by making an IPC request to the rights
 * server. If the right with a given name is found, its number is immediately returned. Otherwise,
 * the behaviour is dependent on the flags. If FLAG_NOBLOCK is set, then the error is returned.
 * Otherwise, the process is blocked untill the right becomes available.
 *
 * @param name The name of the right.
 * @param length The length of the name.
 * @param flags OR of different flags defining the behaviour. FLAG_NOBLOCK indicated that the
 * function should not block. Other bits are currently unused and must be set to 0.
 * @return right_request_t Result of the operation. On success, *right* holds the ID of the right to
 * which the name is asigned.
 */
right_request_t get_right_by_name(const char *name, size_t length, uint32_t flags);

/**
 * @brief Set the port to which the kernel sends its logs.
 *
 * This system call sets the port to which the kernel should send its logs. Untill the port if set
 * or if at some point it becomes invalid, the kernel shall save its logs in an internal buffer. The
 * logs will then be sent as the IPC_Write_Plain messages.
 *
 * @param portnum A valid port to which the logs will be sent.
 * @param flags Currently unused. Must be set to 0.
 * @return result_t Result of the operation.
 * @see IPC_Write_Plain
 */
result_t set_log_port(pmos_port_t portnum, u32 flags);

// Sets right 0 to the given right
// TODO: implement missing stuff (I will not elaborate...)
result_t set_right0(pmos_right_t port);

#endif

// Causes process not to block when the port doesn't exist, returning ERROR_PORT_DOESNT_EXIST
// instead
#define FLAG_NOBLOCK 0x01

#if defined(__cplusplus)
}
#endif

#endif