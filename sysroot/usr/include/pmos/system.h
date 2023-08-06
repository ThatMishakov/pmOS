#ifndef _SYSTEM_H
#define _SYSTEM_H 1
#include <stdint.h>
#include <stddef.h>
#include "../kernel/syscalls.h"
#include "../kernel/memory.h"
#include "../kernel/messaging.h"
#include "../kernel/types.h"

#ifndef SUCCESS
#define SUCCESS 0
#endif

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
syscall_r syscall(uint64_t call_n, ...);

/**
 * @brief Returns the PID of the callee
 * 
 * @return uint64_t PID of the process
 */
uint64_t getpid();

/**
 * @brief Creates a new process
 * 
 * This system call creates a new empty user-space process with an empty page table with an initial UNINITED state and returns its PID.
 * To start the execution, one must load the page table with the create_region* calls and running start_process() syscall.
 * 
 * @return syscall_r structure containing the result and the PID of the new process. If the result != SUCCESS, value does not hold a meaningful data.
 * @see start_process();
 */
syscall_r syscall_new_process();

/**
 * @brief Starts the execution of the process
 * 
 * This system call starts the execution of the process with the given PID. The process must be in the UNINITED state and have a valid page table.
 * After this call, the process gets into the RUNNING state and starts executing the code at the given entry point. The stack pointer can be set
 * using init_stack() call.
 * 
 * @param pid PID of the task to start
 * @param entry Entry point of the process
 * @param arg1 First argument to the entry point. On x86_64, it is passed in the RDI register.
 * @param arg2 Second argument to the entry point. On x86_64, it is passed in the RSI register.
 * @param arg3 Third argument to the entry point. On x86_64, it is passed in the RDX register.
 * @return result_t Result of the call. If the result != SUCCESS, the process was not started.
 * @see init_stack() syscall_new_process() asign_page_table()
 */
result_t syscall_start_process(uint64_t pid, uint64_t entry, uint64_t arg1, uint64_t arg2, uint64_t arg3);

/**
 * @brief Gets info about the first message in the given *port* message queue
 * 
 * This syscall checks the message queue and fills the *descr* descriptor with the information about the first (front) message in it.
 * If there are no messages, the behaviour depends on the flags; either the process is blocked or the ERROR_NO_MESSAGES is returned.
 * 
 * This system call does not modify the visible message queue state.
 * 
 * @param descr Pointer to the valid memory region where the memory descriptor information is to be filled
 * @param port The port from which the message info should be gotten. Current task must be the owner of the port.
 * @param flags Different flags that can define the behaviour of the function. Unused bits must be set to 0. Currently supported flags:
 * @param flags 
 * @param flags FLAG_NOBLOCK - Do not block the process if the queue is empty and return an error instead.
 * @return The result of the execution. If the execution was successfull, the descr should have the information about
 *         the front message in the queue       
 */
result_t syscall_get_message_info(Message_Descriptor* descr, pmos_port_t port, uint32_t flags);

/**
 * @brief Get the first message in the queue of the port. It is intended to be chained after syscall_get_message_info()
 * 
 * @param buff The buffer to where the message should written. Callee must ensure it is large enough to hold the message (should be known
 *             from the syscall_get_message_info() )
 * @param port The valid port from which to get the message. Callee must be the port's owner
 * @param args Aguments. MSG_ARG_NOPOP: do not pop the message after executing the command
 * @return result of the execution. On success, buff should contain the message.
 */
result_t get_first_message(char* buff, pmos_port_t port, uint64_t args);

/**
 * @brief Sends the message to the port
 * 
 * This system call sends the message to the specified port. The message can currently be of any size and the kernel does not inspect the passed buffer
 * in any way, though it probably is a **very bad idea** to not at least define the type from IPC_Generic_Msg.
 * 
 * In the current implementation, the messages are coppied to the kernel buffer and are stored in a linked list of an infinite maximum size. Thus, when sending
 * large amounts of data, it might be a better idea to use shared memory or move the memory regions (read pages) around.
 * 
 * @param port The destination port
 * @param size Size of the message in bytes.
 * @param message Message content. Currently, there are no alignment requirements.
 * @return result_t Generic result of the operation
 * @see syscall_get_message_info() get_first_message() transfer_region()
 * @todo Now that I have redone the memory system in the kernel and have (- and at least I think -) cool memory-moving syscalls, it might be a good idea to
 *       provide an interface to move the pages with the messages.
 */
result_t send_message_port(pmos_port_t port, size_t size, const char* message);

/**
 * @brief Chages the tasks' scheduler priority
 * 
 * This system call changes the scheduler priority of the current process.
 * 
 * Currently, the scheduler is implemented with 16 level per-processor ready queues, where in each level the processes are scheduled using a round-robin
 * algorithm with the quantum dependant on the level. The lower value indicates the higher priority, and the processes with priorities 0-2 cannot
 * be stolen during the load balancind done by other threads. As soon as the process with higher priority becomes unblocked, it preempties the current
 * running process and immediately starts the execution untill is gets blocked or exits. Thus, the higher priority is thought for drivers (with high quantum
 * values, so if the driver is running for short bursts, it should almost never be preemptied) and servers.
 * 
 * @param priority The new priority
 * @return result_t result of the operation
 * @todo Setting the lower than current priority is currently semi-broken (though not catastriphically) as I have not yet implemented the logic for that
 */
result_t request_priority(uint64_t priority);

/// Returns the LAPIC id the process is running on when calling the process
u64 get_lapic_id();

/// Type for the task group ID
typedef uint64_t task_group_t;

/**
 * @brief Creates a new task group
 * 
 * This syscall creates a new task group and returns its ID. Every process can be member of any number of task groups and it is up to the 
 * application to decide how to use them. The task groups are currently used to implement the filesystem clients, where the group ID is
 * used as the filesystem consumer ID. The tasks can be added to the group using the add_task_to_group() and removed using the remove_task_from_group()
 * and also queried using the is_task_group_member().
 * 
 * @return syscall_r result of the operation. If the result is SUCCESS, the value contains the ID of the new group.
 * @see add_task_to_group() remove_task_from_group() is_task_group_member()
 */
syscall_r create_task_group();

/**
 * @brief Removes the task from the task group
 * 
 * This syscall removes the task from the task group. If the task is not in the group, the syscall returns ERROR_NOT_IN_GROUP.
 * The task groups with no members are automatically destroyed by the kernel.
 * 
 * @param task_id ID of the task. Takes PID_SELF (0)
 * @param group_id ID of the group
 * @return result_t result of the operation. If the result is SUCCESS, the task was removed from the group.
 * @see create_task_group() add_task_to_group() is_task_group_member()
 */
result_t remove_task_from_group(uint64_t task_id, uint64_t group_id);

/**
 * @brief Check if the task is in the task group
 * 
 * @param task_id ID of the task
 * @param group_id ID of the group
 * @return syscall_r result of the operation. If the result is SUCCESS, the value contains 1 if the task is in the group, 0 otherwise.
 * @see create_task_group() add_task_to_group() remove_task_from_group()
 */
syscall_r is_task_group_member(uint64_t task_id, uint64_t group_id);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif