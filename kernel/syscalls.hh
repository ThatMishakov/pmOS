#pragma once
#include "sched.hh"
#include <kernel/errors.h>
#include <kernel/syscalls.h>
#include "types.hh"


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

extern "C" ReturnStr<uint64_t> syscall_handler(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

#pragma GCC diagnostic pop

// Allocates the page at virtual_addr
uint64_t get_page(uint64_t virtual_addr);

// Releases the page at virtual_addr
uint64_t release_page(uint64_t virtual_addr);

// Gets the pid of the current process
ReturnStr<uint64_t> getpid();

// Creates an empty process
ReturnStr<uint64_t> syscall_create_process();

// Transfers pages to another process
kresult_t syscall_map_into();

// Transfers pages to another processor in range
kresult_t syscall_map_into_range(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

// Blocks current process
ReturnStr<uint64_t> syscall_block(uint64_t mask);

// Allocates the nb_page at virtual_addr
kresult_t syscall_get_page_multi(uint64_t virtual_addr, uint64_t nb_pages);

// Starts a process with PID pid at starting point start
kresult_t syscall_start_process(uint64_t pid, uint64_t start);

// Exits (kills the process at the end of its execution)
kresult_t syscall_exit(uint64_t arg1, uint64_t arg2);

// Maps physical memory into process
kresult_t syscall_map_phys(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

// Get info about the last message
kresult_t syscall_get_message_info(uint64_t message_struct);

// Gets first message in the messaging queue
kresult_t syscall_get_first_message(uint64_t buff, uint64_t args);

// Sends a message to the process pid at channel *channel*
kresult_t syscall_send_message_task(uint64_t pid, uint64_t channel, uint64_t size, uint64_t message);

// Sends a message to the port
kresult_t syscall_send_message_port(uint64_t port, size_t size, uint64_t message);

// Sets a task's port
kresult_t syscall_set_port(uint64_t pid, uint64_t port, uint64_t dest_pid, uint64_t dest_chan);

// Sets task's attributes
kresult_t syscall_set_attribute(uint64_t pid, uint64_t attribute, uint64_t value); 