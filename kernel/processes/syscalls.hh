#pragma once
#include <processes/sched.hh>
#include <kernel/errors.h>
#include <kernel/syscalls.h>
#include <types.hh>


//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

extern "C" ReturnStr<u64> syscall_handler(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6);

//#pragma GCC diagnostic pop

// Allocates the page at virtual_addr
kresult_t get_page(u64 virtual_addr);

// Releases the page at virtual_addr
kresult_t release_page(u64 virtual_addr);

// Gets the pid of the current process
ReturnStr<u64> getpid();

// Creates an empty process
ReturnStr<u64> syscall_create_process();

// Transfers pages to another process
kresult_t syscall_map_into();

// Transfers pages to another processor in range
kresult_t syscall_map_into_range(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

// Blocks current process
ReturnStr<u64> syscall_block(u64 mask);

// Allocates the nb_page at virtual_addr
kresult_t syscall_get_page_multi(u64 virtual_addr, u64 nb_pages);

// Releases the nb_page at virtual_addr
kresult_t syscall_release_page_multi(u64 virtual_addr, u64 nb_pages);

// Starts a process with PID pid at starting point start
kresult_t syscall_start_process(u64 pid, u64 start, u64 arg1, u64 arg2, u64 arg3);

// Exits (kills the process at the end of its execution)
kresult_t syscall_exit(u64 arg1, u64 arg2);

// Maps physical memory into process
kresult_t syscall_map_phys(u64 arg1, u64 arg2, u64 arg3, u64 arg4);

// Get info about the last message
kresult_t syscall_get_message_info(u64 message_struct);

// Gets first message in the messaging queue
kresult_t syscall_get_first_message(u64 buff, u64 args);

// Sends a message to the process pid at channel *channel*
kresult_t syscall_send_message_task(u64 pid, u64 channel, u64 size, u64 message);

// Sends a message to the port
kresult_t syscall_send_message_port(u64 port, size_t size, u64 message);

// Sets a task's port
kresult_t syscall_set_port(u64 pid, u64 port, u64 dest_pid, u64 dest_chan);

// Sets kernel's port
kresult_t syscall_set_port_kernel(u64 port, u64 dest_pid, u64 dest_chan);

// Sets task's attributes
kresult_t syscall_set_attribute(u64 pid, u64 attribute, u64 value); 

// Inits task's stack
ReturnStr<u64> syscall_init_stack(u64 pid, u64 esp);

// Shares *nb_pages* pages starting with alligned *page_start* to process *pid* at addr *to_addr* with *flags* found in kernel/flags.h
kresult_t syscall_share_with_range(u64 pid, u64 page_start, u64 to_addr, u64 nb_pages, u64 flags);