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

#ifndef _SYSTEM_H
#define _SYSTEM_H 1
#include "../kernel/memory.h"
#include "../kernel/messaging.h"
#include "../kernel/syscalls.h"
#include "../kernel/types.h"

#include <stddef.h>
#include <stdint.h>

#ifndef SUCCESS
    #define SUCCESS 0
#endif

#define NO_CPU      0
#define CURRENT_CPU (uint64_t)(-1)

typedef uint64_t result_t;
typedef uint64_t pmos_port_t;

typedef struct {
    result_t result;
    uint64_t value;
} syscall_r;

#define INVALID_PORT 0

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

// Generic syscall
syscall_r pmos_syscall(uint64_t call_n, ...);

/**
 * @brief Returns the PID of the callee
 *
 * @return uint64_t PID of the process
 */
uint64_t get_task_id();

/**
 * @brief Creates a new process
 *
 * This system call creates a new empty user-space process with an empty page table with an initial
 * UNINITED state and returns its PID. To start the execution, one must load the page table with the
 * create_region* calls and running start_process() syscall.
 *
 * @return syscall_r structure containing the result and the PID of the new process. If the result
 * != SUCCESS, value does not hold a meaningful data.
 * @see start_process();
 */
syscall_r syscall_new_process();

/**
 * @brief Kills the task
 * 
 * @param tid ID of the task to kill
 * @return result_t result of the operation
 */
result_t syscall_kill_task(uint64_t tid);

/**
 * @brief Starts the execution of the process
 *
 * This system call starts the execution of the process with the given PID. The process must be in
 * the UNINITED state and have a valid page table. After this call, the process gets into the
 * RUNNING state and starts executing the code at the given entry point. The stack pointer can be
 * set using init_stack() call.
 *
 * @param pid PID of the task to start
 * @param entry Entry point of the process
 * @param arg1 First argument to the entry point. On x86_64, it is passed in the RDI register.
 * @param arg2 Second argument to the entry point. On x86_64, it is passed in the RSI register.
 * @param arg3 Third argument to the entry point. On x86_64, it is passed in the RDX register.
 * @return result_t Result of the call. If the result != SUCCESS, the process was not started.
 * @see init_stack() syscall_new_process() asign_page_table()
 */
result_t syscall_start_process(uint64_t pid, unsigned long entry, unsigned long arg1, unsigned long arg2,
                               unsigned long arg3);

/**
 * @brief Loads an executable from the memory object into the address space of a task
 *
 * This system call loads an executable from a given memory object into the address space of a given
 * task. Currently, ELF format is supported. The task must not have a page table assigned. The
 * shared memory is used for mapping the data.
 *
 * @param tid ID of the task to load the executable into
 * @param object_id ID of the memory object containing the executable
 * @param flags Flags for the function. Currently unused, but at least FLAG_NOBLOCK is planned
 * @return result_t Result of the operation. If the result != SUCCESS, the executable was not
 * loaded.
 * @see syscall_start_process()
 * @todo Think of a way to pass the arguments to the executable
 */
result_t syscall_load_executable(uint64_t tid, uint64_t object_id, uint32_t flags);

/// Sets the name of the task
result_t syscall_set_task_name(uint64_t tid, const char *name, size_t name_length);

/**
 * @brief Gets info about the first message in the given *port* message queue
 *
 * This syscall checks the message queue and fills the *descr* descriptor with the information about
 * the first (front) message in it. If there are no messages, the behaviour depends on the flags;
 * either the process is blocked or the ERROR_NO_MESSAGES is returned.
 *
 * This system call does not modify the visible message queue state.
 *
 * @param descr Pointer to the valid memory region where the memory descriptor information is to be
 * filled
 * @param port The port from which the message info should be gotten. Current task must be the owner
 * of the port.
 * @param flags Different flags that can define the behaviour of the function. Unused bits must be
 * set to 0. Currently supported flags:
 * @param flags
 * @param flags FLAG_NOBLOCK - Do not block the process if the queue is empty and return an error
 * instead.
 * @return The result of the execution. If the execution was successfull, the descr should have the
 * information about the front message in the queue
 */
result_t syscall_get_message_info(Message_Descriptor *descr, pmos_port_t port, uint32_t flags);

/**
 * @brief Get the first message in the queue of the port. It is intended to be chained after
 * syscall_get_message_info()
 *
 * @param buff The buffer to where the message should written. Callee must ensure it is large enough
 * to hold the message (should be known from the syscall_get_message_info() )
 * @param port The valid port from which to get the message. Callee must be the port's owner
 * @param args Aguments. MSG_ARG_NOPOP: do not pop the message after executing the command
 * @return result of the execution. On success, buff should contain the message.
 */
result_t get_first_message(char *buff, uint32_t args, pmos_port_t port);

/**
 * @brief Sends the message to the port
 *
 * This system call sends the message to the specified port. The message can currently be of any
 * size and the kernel does not inspect the passed buffer in any way, though it probably is a **very
 * bad idea** to not at least define the type from IPC_Generic_Msg.
 *
 * In the current implementation, the messages are coppied to the kernel buffer and are stored in a
 * linked list of an infinite maximum size. Thus, when sending large amounts of data, it might be a
 * better idea to use shared memory or move the memory regions (read pages) around.
 *
 * @param port The destination port
 * @param size Size of the message in bytes.
 * @param message Message content. Currently, there are no alignment requirements.
 * @return result_t Generic result of the operation
 * @see syscall_get_message_info() get_first_message() transfer_region()
 * @todo Now that I have redone the memory system in the kernel and have (- and at least I think -)
 * cool memory-moving syscalls, it might be a good idea to provide an interface to move the pages
 * with the messages.
 */
result_t send_message_port(pmos_port_t port, size_t size, const void *message);

/**
 * @brief Chages the tasks' scheduler priority
 *
 * This system call changes the scheduler priority of the current process.
 *
 * Currently, the scheduler is implemented with 16 level per-processor ready queues, where in each
 * level the processes are scheduled using a round-robin algorithm with the quantum dependant on the
 * level. The lower value indicates the higher priority, and the processes with priorities 0-2
 * cannot be stolen during the load balancind done by other threads. As soon as the process with
 * higher priority becomes unblocked, it preempties the current running process and immediately
 * starts the execution untill is gets blocked or exits. Thus, the higher priority is thought for
 * drivers (with high quantum values, so if the driver is running for short bursts, it should almost
 * never be preemptied) and servers.
 *
 * @param priority The new priority
 * @return result_t result of the operation
 * @todo Setting the lower than current priority is currently semi-broken (though not
 * catastriphically) as I have not yet implemented the logic for that
 */
result_t request_priority(uint32_t priority);

/**
 * @brief Sets the affinity of the task to the given CPU
 *
 * This system call allows to bind a given task to the given CPU. Additionally, NO_CPU can be passed
 * to unbind the task from any concrete CPU and allow kernel to freely schedule it across
 * processors. Also, CURRENT_CPU can be passed to bind the task to the CPU where the task is
 * currently running.
 *
 * @param tid ID of the task to bind. Takes TASK_ID_SELF (0). Currently, only the current task can
 * be bound.
 * @param cpu_id ID of the CPU to bind the task to. NO_CPU unbinds the task, CURRENT_CPU binds the
 * task to the current CPU. If the task is not bound to any CPU and is not running, the kernel will
 * bind it to the CPU of the system call caller.
 * @param flags Flags for the operation. Currently unused, must be set to 0.
 * @return result_t result of the operation
 */
result_t set_affinity(uint64_t tid, uint32_t cpu_id, uint32_t flags);

/// Returns the LAPIC id the process is running on when calling the process
/// This value is not shifted left, so in non-X2APIC it would be lapic id << 24
/// @param cpu_num Index of the CPU (assigned by the kernel during its initialization).
///                0 counts as the CPU the caller is executing on.
syscall_r get_lapic_id(uint32_t cpu_id);

/// Type for the task group ID
typedef uint64_t task_group_t;

/**
 * @brief Creates a new task group
 *
 * This syscall creates a new task group and returns its ID. Every process can be member of any
 * number of task groups and it is up to the application to decide how to use them. The task groups
 * are currently used to implement the filesystem clients, where the group ID is used as the
 * filesystem consumer ID. The tasks can be added to the group using the add_task_to_group() and
 * removed using the remove_task_from_group() and also queried using the is_task_group_member().
 *
 * @return syscall_r result of the operation. If the result is SUCCESS, the value contains the ID of
 * the new group.
 * @see add_task_to_group() remove_task_from_group() is_task_group_member()
 */
syscall_r create_task_group();

/**
 * @brief Adds the task to the task group
 *
 * This syscall adds the task to the task group. If the task is already in the group, the syscall
 * returns ERROR_ALREADY_IN_GROUP. If the same task is added or removed from the same group by
 * multiple threads at the same time, the return value might not reflect the actual state of the
 * task group.
 *
 * @param task_id ID of the task to be added. Takes TASK_ID_SELF (0)
 * @param group_id ID of the group where the task should be added
 * @return result_t Result of the operation. If the result is SUCCESS, the task was added to the
 * group.
 */
result_t add_task_to_group(uint64_t task_id, uint64_t group_id);

/**
 * @brief Removes the task from the task group
 *
 * This syscall removes the task from the task group. If the task is not in the group, the syscall
 * returns ERROR_NOT_IN_GROUP. The task groups with no members are automatically destroyed by the
 * kernel.
 *
 * @param task_id ID of the task. Takes TASK_ID_SELF (0)
 * @param group_id ID of the group
 * @return result_t result of the operation. If the result is SUCCESS, the task was removed from the
 * group.
 * @see create_task_group() add_task_to_group() is_task_group_member()
 */
result_t remove_task_from_group(uint64_t task_id, uint64_t group_id);

/**
 * @brief Check if the task is in the task group
 *
 * @param task_id ID of the task
 * @param group_id ID of the group
 * @return syscall_r result of the operation. If the result is SUCCESS, the value contains 1 if the
 * task is in the group, 0 otherwise.
 * @see create_task_group() add_task_to_group() remove_task_from_group()
 */
syscall_r is_task_group_member(uint64_t task_id, uint64_t group_id);

    /// Mask bit for when the task group is destroyed
    #define NOTIFICATION_MASK_DESTROYED 0x01
    /// Mask bit for when a task is removed from the task group
    #define NOTIFICATION_MASK_ON_REMOVE_TASK 0x02
    /// Mask bit for when a task is added to the task group
    #define NOTIFICATION_MASK_ON_ADD_TASK 0x04

    /// Flag to notify for the existing tasks
    #define NOTIFY_FOR_EXISTING_TASKS 0x01
/**
 * @brief Sets the notification mask for the given task group
 *
 * This function allows to set up notifications for the task group. They are done by assigning
 * messaging port and a mask to the group, which in turn causes kernel to send the notification
 * messages to the port when an event is triggered.
 *
 * There is no remove function, as setting a 0 as a mask removes notification checking and frees up
 * the resources needed to have port watchers.
 *
 * Internally, this is implemented as an unbound list-like structure, storing ports where the
 * notifications are needed to be sent. Thus, any number of ports can watch the task group, though
 * it is advised to not overuse this function to avoid slowdowns of operations involving
 * modification of task group state.
 *
 * The mask is a bitmap of events that should send messages. To avoid the problems with future
 * compaitability, bits that have no function should be set to 0.
 *
 * Upon the execution, the previous mask is returned. The function is atomic and calling it
 * concurrently will not corrupt kernel's internal state, though programmer should be wary of
 * possible race conditions, since the kernel does not give guaranties on the execution order of
 * concurrent operations.
 *
 * @param task_group_id ID of the task group
 * @param port_id ID of the port where the notifications must be sent
 * @param new_mask New mask of the notifier, which replaces the old one. 0 removes the port from the
 * watchers list.
 * @param flags Flags for the operation. Can take NOTIFY_FOR_EXISTING_TASKS to send the notifications
 * for the tasks that are already in the group.
 * @return syscall_r result. If the result is SUCCESS, the value contains the old mask. Otherwise,
 * the value is undefined.
 */
syscall_r set_task_group_notifier_mask(uint64_t task_group_id, pmos_port_t port_id,
                                       uint32_t new_mask, uint32_t flags);

/**
 * @brief Yields the CPU to the next task
 *
 * This system call executes the kernel's scheduler, which potentially selects and runs a different
 * task, putting the caller into the ready queue.
 *
 * @return result_t result of the operation
 */
result_t pmos_yield();

#define GET_TIME_NANOSECONDS_SINCE_BOOTUP 0
#define GET_TIME_REALTIME_NANOSECONDS     1
/**
 * @brief Gets the system time
 * 
 * This function gets the system time, according to the mode parameter. The following modes are supported:
 * - GET_TIME_NANOSECONDS_SINCE_BOOTUP: Returns the number of ticks since the bootup of the system
 * @param mode Mode of the operation
 * @return syscall_r result of the operation. If the result is SUCCESS, the value contains the time
*/
syscall_r pmos_get_time(unsigned mode);

result_t request_timer(pmos_port_t port, uint64_t after_ns);

result_t request_named_port(const char *name, size_t name_length, pmos_port_t reply_port, uint32_t flags);

result_t pause_task(uint64_t tid);

result_t resume_task(uint64_t tid);

int pmos_request_timer(pmos_port_t port, uint64_t ns, uint64_t extra);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif