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

// Allocates the page at virtual_addr
void get_page(u64 virtual_addr);

// Releases the page at virtual_addr
void release_page(u64 virtual_addr);

// Gets the pid of the current process
void getpid();

// Creates an empty process
void syscall_create_process();

// Transfers pages to another process
void syscall_map_into();

// Transfers pages to another processor in range
void syscall_map_into_range(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

// Blocks current process
void syscall_block(u64 mask);

// Allocates the nb_page at virtual_addr
void syscall_get_page_multi(u64 virtual_addr, u64 nb_pages);

// Releases the nb_page at virtual_addr
void syscall_release_page_multi(u64 virtual_addr, u64 nb_pages);

// Starts a process with PID pid at starting point start
void syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3);

// Exits (kills the process at the end of its execution)
void syscall_exit(u64 arg1, u64 arg2);

// Maps physical memory into process
void syscall_map_phys(u64 arg1, u64 arg2, u64 arg3, u64 arg4);

// Get info about the last message
void syscall_get_message_info(u64 message_struct);

// Gets first message in the messaging queue
void syscall_get_first_message(u64 buff, u64 args);

// Sends a message to the process pid at channel *channel*
void syscall_send_message_task(u64 pid, u64 channel, u64 size, u64 message);

// Sends a message to the port
void syscall_send_message_port(u64 port, size_t size, u64 message);

// Sets a task's port
void syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan);

// Sets kernel's port
void syscall_set_port_kernel(u64 port, u64 dest_pid, u64 dest_chan);

// Sets default port
void syscall_set_port_default(u64 port, u64 dest_pid, u64 dest_chan);

// Sets task's attributes
void syscall_set_attribute(u64 pid, u64 attribute, u64 value); 

// Inits task's stack
void syscall_init_stack(u64 pid, u64 esp);

// Shares *nb_pages* pages starting with alligned *page_start* to process *pid* at addr *to_addr* with *flags* found in kernel/flags.h
void syscall_share_with_range(u64 pid, u64 page_start, u64 to_addr, u64 nb_pages, u64 flags);

// Checks if the page is allocated for user process
void syscall_is_page_allocated(u64 page);

// Returns LAPIC id of the current CPU
void syscall_get_lapic_id();

// Programs interrupt to send the message to the right port
void syscall_set_interrupt(uint64_t port, uint32_t intno, uint32_t flags);

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