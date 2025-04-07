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

#pragma once
#include <kernel/syscalls.h>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>

// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

extern "C" void syscall_handler();

// #pragma GCC diagnostic pop

// Gets the pid of the current process
void syscall_get_task_id();
// No parameters

// Creates an empty process
void syscall_create_process();
// No parameters

// Creates a normal (delayed allocation region)
void syscall_create_normal_region();
// Parameters: u64 pid, u64 addr_start, u64 size, u64 access_flags

// Creates a region mapped to phys_addr
void syscall_create_phys_map_region();
// Parameters: u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr

// Starts a process with PID pid at starting point start
void syscall_start_process();
// Parameters: u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3

// Gets an index of the processes' page table. PID 0 can be used as TASK_ID_SELF
void syscall_get_page_table();
// Parameters: u64 pid

// Exits (kills the process at the end of its execution)
void syscall_exit();
// Parameters: u64 arg1, u64 arg2

// Get info about the last message
void syscall_get_message_info();
// Parameters: u64 message_struct, u64 portno, u64 flags

// Gets first message in the messaging queue
void syscall_get_first_message();
// Parameters: u64 buff, u64 args, u64 portno

// Sends a message to the port
void syscall_send_message_port();
// Parameters: u64 port, size_t size, u64 message

// Sets a task's port
void syscall_set_port();
// Parameters: u64 pid, u64 port, u64 dest_pid, u64 dest_chan

// Sets task's attributes
void syscall_set_attribute();
// Parameters: u64 pid, u64 attribute, u64 value

// Initializes task's stack
void syscall_init_stack();
// Parameters: u64 pid, u64 esp

// Checks if the page is allocated for user process
void syscall_is_page_allocated();
// Parameters: u64 page

// Returns LAPIC id of the current CPU
void syscall_get_lapic_id();
// No parameters

// Programs interrupt to send the message to the right port
void syscall_set_interrupt();
// Parameters: uint64_t port, u64 intno, u64 flags

// Assigns a name to port
void syscall_name_port();
// Parameters: u64 port, const char* name, u64 length, u64 flags

// Gets port by its name
void syscall_get_port_by_name();
// Parameters: const char* name, u64 length, u64 flags

// Sets the kernel's log port
void syscall_set_log_port();
// Parameters: u64 port, u64 flags

// Requests a port by its name in a non-blocking way and sends a message with the descriptor when it
// becomes available
void syscall_request_named_port();
// Parameters: const char* string_ptr, u64 length, u64 reply_chan, u64 flags

// Transfers a memory region to a new page table
void syscall_transfer_region();
// Parameters: u64 to_page_table, u64 region, u64 dest, u64 flags

// Sets segment registers for the task
void syscall_set_segment();
// Parameters: u64 pid, u64 segment_type, u64 ptr

// Gets segment registers for the task
void syscall_get_segment();
// Parameters: u64 pid, u64 segment_type

void syscall_asign_page_table();
// Parameters: u64 pid, u64 page_table, u64 flags

void syscall_create_mem_object();
// Parameters: u64 size_bytes

// Deletes a region identified by region_start
void syscall_delete_region();
// Parameters: u64 region_start

// Loads the ELF executable into the task
void syscall_load_executable();
// Parameters: u64 task_id, u64 object_id, u64 flags

#define SYS_CONF_IOAPIC  0x01
#define SYS_CONF_LAPIC   0x02
#define SYS_CONF_CPU     0x03
#define SYS_CONF_SLEEP10 0x04

// Configures a system
void syscall_configure_system();
// Parameters: u64 type, u64 arg1, u64 arg2

void syscall_set_priority();
// Parameters: u64 priority

void syscall_set_task_name();
// Parameters: u64 pid, const char* string, u64 length

void syscall_create_port();
// Parameters: u64 owner

// Creates a new task group. Adds current task to it.
void syscall_create_task_group();
// No parameters

// Adds a task to a task group
void syscall_add_to_task_group();
// Parameters: u64 pid, u64 group

// Removes a task from a task group
void syscall_remove_from_task_group();
// Parameters: u64 pid, u64 group

// Checks if a task is in a task group
void syscall_is_in_task_group();
// Parameters: u64 pid, u64 group

// Sets up the notification mask for the given task group for the port
void syscall_set_notify_mask();
// Parameters: u64 task_group, u64 port_id, u64 new_mask

// Requests a timer notification on a given port after a given timeout
void syscall_request_timer();
// Parameters: u64 port, u64 timeout

// Sets the task's affinity
void syscall_set_affinity();
// Parameters: u64 task_id, u64 cpu, u64 flags

// Completes an interrupt, dispatched to the user space
void syscall_complete_interrupt();
// Parameters: u64 intno

// Yields to the next task (if there is some)
// In other words, reschedule()s
void syscall_yield();
// No parameters

// Maps a memory object to the task's address space
void syscall_map_mem_object();
// Parameters: u64 page_table_id, u64 addr_start, u64 size_bytes, u64 access, u64 object_id, u64
// offset

void syscall_pause_task();
// Parameters: u64 task_id

void syscall_get_time();
// Parameters: u64 mode

void syscall_system_info();
// Parameters: u64 info_type

void syscall_kill_task();
// Parameters: u64 task_id

void syscall_resume_task();
// Parameters: u64 task_id

void syscall_unmap_range();
// Parameters: u64 task_id, u64 addr_start, u64 size

void syscall_get_page_address();
// Parameters: u64 task_id, u64 page_base, u64 flags

void syscall_get_page_address_from_object();
// Parameters: u64 object_id, u64 offset, u64 flags

void syscall_unreference_mem_object();
// Parameters: u64 object_id

void syscall_cpu_for_interrupt();
// Parameters: u32 gsi, flags

void syscall_set_port0();

struct SyscallRetval {
    TaskDescriptor *task;
    u64 operator=(u64 value);
    operator u64() const;
};
struct SyscallError {
    TaskDescriptor *task;
    i64 operator=(i64 value);
    operator int() const;
};
inline SyscallError syscall_error(TaskDescriptor *task) { return {task}; }
inline SyscallRetval syscall_return(TaskDescriptor *task) { return {task}; }

ulong syscall_flags(TaskDescriptor *task);
ulong syscall_flags_reg(TaskDescriptor *task);
unsigned syscall_number(TaskDescriptor *task);
ulong syscall_arg(TaskDescriptor *task, int arg, int args64before = 0);
u64 syscall_arg64(TaskDescriptor *task, int arg);
void syscall_success(TaskDescriptor *task);

ReturnStr<bool> syscall_arg64_checked(TaskDescriptor *task, int arg, u64 &value);
ReturnStr<bool> syscall_arg_checked(TaskDescriptor *task, int arg, int args64before, ulong &value);
ReturnStr<bool> syscall_args_checked(TaskDescriptor *task, int arg, int args64before, int count, ulong *values);

// Entry point for when userspace calls SYSCALL instruction
extern "C" void syscall_entry();

// Entry point for when userspace calls SYSENTER instruction
extern "C" void sysenter_entry();

// Entry point for when userspace calls software interrupt
extern "C" void syscall_int_entry();

// Enables SYSCALL instruction
void program_syscall();