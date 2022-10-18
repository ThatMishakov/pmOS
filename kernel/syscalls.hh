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
ReturnStr<uint64_t> syscall_create_process();

// Transfers pages to another process
kresult_t syscall_map_into();

// Transfers pages to another processor in range
kresult_t syscall_map_into_range(TaskDescriptor*);

// Blocks current process
ReturnStr<uint64_t> syscall_block(TaskDescriptor* current);