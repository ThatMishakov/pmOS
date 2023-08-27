#pragma once
#include <sched/sched.hh>
#include <kernel/errors.h>
#include <kernel/syscalls.h>
#include <types.hh>
#include <processes/tasks.hh>


//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

extern "C" void syscall_handler();

//#pragma GCC diagnostic pop

// Gets the pid of the current process
void getpid(u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Creates an empty process
void syscall_create_process(u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Creates a normal (dellayed allocation region)
void syscall_create_normal_region(u64 pid, u64 addr_start, u64 size, u64 access_flags, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Creates a region mapped to phys_addr
void syscall_create_phys_map_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr, u64 = 0 /* unused */);


// Starts a process with PID pid at starting point start
void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3, u64 = 0 /* unused */);


// Gets an index of the processes' page table. PID 0 can be used as PID_SELF
void syscall_get_page_table(u64 pid, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Exits (kills the process at the end of its execution)
void syscall_exit(u64 arg1, u64 arg2, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Get info about the last message
void syscall_get_message_info(u64 message_struct, u64 portno, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Gets first message in the messaging queue
void syscall_get_first_message(u64 buff, u64 args, u64 portno, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sends a message to the port
void syscall_send_message_port(u64 port, size_t size, u64 message, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sets a task's port
void syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sets task's attributes
void syscall_set_attribute(u64 pid, u64 attribute, u64 value, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */); 

// Inits task's stack
void syscall_init_stack(u64 pid, u64 esp, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Checks if the page is allocated for user process
void syscall_is_page_allocated(u64 page, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Returns LAPIC id of the current CPU
void syscall_get_lapic_id(u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Programs interrupt to send the message to the right port
void syscall_set_interrupt(uint64_t port, u64 intno, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Assigns a name to port
void syscall_name_port(u64 port, u64 /* const char* */ name, u64 length, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Gets port by its name
void syscall_get_port_by_name(u64 /* const char * */ name, u64 length, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sets the kernel's log port
void syscall_set_log_port(u64 port, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Requests a port by its name in a non-blocking way and sends a message with the descriptor when it becomes available
void syscall_request_named_port(u64 string_ptr, u64 length, u64 reply_chan, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Transfers a memory region to a new page table
void syscall_transfer_region(u64 to_page_table, u64 region, u64 dest, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sets segment registers for the task
void syscall_set_segment(u64 pid, u64 segment_type, u64 ptr, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Gests segment registers for the task
void syscall_get_segment(u64 pid, u64 segment_type, u64 = 0, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

void syscall_asign_page_table(u64 pid, u64 page_table, u64 flags, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

void syscall_create_mem_object(u64 size_bytes, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Deletes a region identified by region_start
void syscall_delete_region(u64 region_start, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

#define SYS_CONF_IOAPIC          0x01
#define SYS_CONF_LAPIC           0x02
#define SYS_CONF_CPU             0x03
#define SYS_CONF_SLEEP10         0x04

// Configures a system
void syscall_configure_system(u64 type, u64 arg1, u64 arg2, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

void syscall_set_priority(u64 priority, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

void syscall_set_task_name(u64 pid, u64 /* const char* */ string, u64 length, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

void syscall_create_port(u64 owner, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Creates a new task group. Adds current task to it.
void syscall_create_task_group(u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Adds a task to a task group
void syscall_add_to_task_group(u64 pid, u64 group, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Removes a task from a task group
void syscall_remove_from_task_group(u64 pid, u64 group, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Checks if a task is in a task group
void syscall_is_in_task_group(u64 pid, u64 group, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */, u64 = 0 /* unused */);

// Sets up the notification mask for the given task group for the port
void syscall_set_notify_mask(u64 task_group, u64 port_id, u64 new_mask, u64, u64, u64);


inline u64& syscall_arg1(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.rsi;
}

inline u64& syscall_arg2(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.rdx;
}

inline u64& syscall_arg3(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.rcx;
}

inline u64& syscall_arg4(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.r8;
}

inline u64& syscall_arg5(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.r9;
}

inline kresult_t& syscall_ret_low(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.rax;
}

inline u64& syscall_ret_high(const klib::shared_ptr<TaskDescriptor>& task)
{
    return task->regs.scratch_r.rdx;
}

// Entry point for when userpsace calls SYSCALL instruction
extern "C" void syscall_entry();

// Entry point for when userpsace calls SYSENTER instruction
extern "C" void sysenter_entry();

// Entry point for when userpsace calls software interrupt
extern "C" void syscall_int_entry();

// Enables SYSCALL instruction
void program_syscall();