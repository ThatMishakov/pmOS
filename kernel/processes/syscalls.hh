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
void getpid();

// Creates an empty process
void syscall_create_process();

// Creates a normal (dellayed allocation region)
void syscall_create_normal_region(u64 pid, u64 addr_start, u64 size, u64 access_flags);

// Creates a managed region
void syscall_create_managed_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 port);

// Creates a region mapped to phys_addr
void syscall_create_phys_map_region(u64 pid, u64 addr_start, u64 size, u64 access, u64 phys_addr);


// Starts a process with PID pid at starting point start
void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3);


// Gets an index of the processes' page table. PID 0 can be used as PID_SELF
void syscall_get_page_table(u64 pid);

// Exits (kills the process at the end of its execution)
void syscall_exit(u64 arg1, u64 arg2);

// Get info about the last message
void syscall_get_message_info(u64 message_struct, u64 portno, u32 flags);

// Gets first message in the messaging queue
void syscall_get_first_message(u64 buff, u64 args, u64 portno);

// Sends a message to the port
void syscall_send_message_port(u64 port, size_t size, u64 message);

// Sets a task's port
void syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan);

// Sets task's attributes
void syscall_set_attribute(u64 pid, u64 attribute, u64 value); 

// Inits task's stack
void syscall_init_stack(u64 pid, u64 esp);

// Checks if the page is allocated for user process
void syscall_is_page_allocated(u64 page);

// Returns LAPIC id of the current CPU
void syscall_get_lapic_id();

// Programs interrupt to send the message to the right port
void syscall_set_interrupt(uint64_t port, uint32_t intno, uint32_t flags);

// Assigns a name to port
void syscall_name_port(u64 port, const char* name, u64 length, u32 flags);

// Gets port by its name
void syscall_get_port_by_name(const char *name, u64 length, u32 flags);

// Sets the kernel's log port
void syscall_set_log_port(u64 port, u32 flags);

// Requests a port by its name in a non-blocking way and sends a message with the descriptor when it becomes available
void syscall_request_named_port(u64 string_ptr, u64 length, u64 reply_chan, u32 flags);

// Provide a page for a managed region
void syscall_provide_page(u64 page_table, u64 dest_page, u64 source, u64 flags);

#define SYS_CONF_IOAPIC          0x01
#define SYS_CONF_LAPIC           0x02
#define SYS_CONF_CPU             0x03
#define SYS_CONF_SLEEP10         0x04

// Configures a system
void syscall_configure_system(u64 type, u64 arg1, u64 arg2);

void syscall_set_priority(u64 priority);

void syscall_set_task_name(u64 pid, const char* string, u64 length);

void syscall_create_port(u64 owner);


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