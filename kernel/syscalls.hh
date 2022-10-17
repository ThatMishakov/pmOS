#pragma once
#include "sched.hh"
#include "common/errors.h"
#include "common/syscalls.h"
#include "types.hh"

void syscall_handler(TaskDescriptor* task);

uint64_t get_page(uint64_t virtual_addr);
uint64_t release_page(uint64_t virtual_addr);
ReturnStr<uint64_t> getpid(TaskDescriptor*);
ReturnStr<uint64_t> syscall_create_process();