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
 * @brief Assigns a name to the unnamed port.
 *
 * This syscall assigns a name to the IPC port. Internally, it creates a Named_Port object, which
 * points to a port. The named port must not exist, otherwise the error will be returned. The tasks
 * might then reference this name to get the underlying IPC port. In addition, during the creation
 * of the object, if there are waiting tasks, they immediately get unblocked and recieve the new
 * port number. See get_port_by_name().
 *
 * @param portnum The ID of the IPC port to which the name would be asigned.
 * @param name The pointer to the string holding the port name. Any character is valid, including
 * '\0'. The kernel uses binary comparison to determine if the name matches and does stop on NULL
 * termination.
 * @param length The length of the name. In current implementation, there is no limit.
 * @param flags Currently unused. Must be set to 0.
 * @return result_t Result of the operation.
 * @todo Create a syscall to delete the named ports. Decide if a port might have several names.
 * @see get_port_by_name()
 */
result_t name_port(pmos_port_t portnum, const char *name, size_t length, uint32_t flags);

/**
 * @brief Get the port by name object.
 *
 * This system call gets a port referenced by the *name*. If the port with a given name is found,
 * its number is immediately returned. Otherwise, the behaviour is dependent on the flags. If
 * FLAG_NOBLOCK is set, then the error is returned. Otherwise, the process is blocked untill the
 * port becomes available.
 *
 * @param name The name of the port. Similarly to name_port(pmos_port_t, const char*, size_t,
 * uint32_t), any character is valid and '\0' is not considered a string termination.
 * @param length The length of the name.
 * @param flags OR of different flags defining the behaviour. FLAG_NOBLOCK indicated that the
 * syscall should not block. Other bits are currently unused and must be set to 0.
 * @return ports_request_t Result of the operation. On success, *port* holds the ID of the port to
 * which the name is asigned.
 */
ports_request_t get_port_by_name(const char *name, size_t length, uint32_t flags);

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

#endif

// Causes process not to block when the port doesn't exist, returning ERROR_PORT_DOESNT_EXIST
// instead
#define FLAG_NOBLOCK 0x01

#if defined(__cplusplus)
}
#endif

#endif