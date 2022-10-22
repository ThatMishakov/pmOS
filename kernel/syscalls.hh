#pragma once
#include "sched.hh"
#include "common/errors.h"
#include "common/syscalls.h"
#include "types.hh"

void syscall_handler(TaskDescriptor* task);

// Allocates the page at virtual_addr
uint64_t get_page(uint64_t virtual_addr);

// Releases the page at virtual_addr
uint64_t release_page(uint64_t virtual_addr);

// Gets the pid of the current process
ReturnStr<uint64_t> getpid(TaskDescriptor*);

// Creates an empty process
ReturnStr<uint64_t> syscall_create_process(uint8_t ring);

// Transfers pages to another process
kresult_t syscall_map_into();

// Transfers pages to another processor in range
kresult_t syscall_map_into_range(TaskDescriptor*);

// Blocks current process
ReturnStr<uint64_t> syscall_block(TaskDescriptor* current);

// Allocates the nb_page at virtual_addr
kresult_t syscall_get_page_multi(uint64_t virtual_addr, uint64_t nb_pages);

// Starts a process with PID pid at starting point start
kresult_t syscall_start_process(uint64_t pid, uint64_t start);

// Exits (kills the process at the end of its execution)
kresult_t syscall_exit(TaskDescriptor* task);

// Maps physical memory into process
kresult_t syscall_map_phys(TaskDescriptor*);