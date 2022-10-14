#pragma once
#include "sched.hh"
#include "common/errors.h"
#include "common/syscalls.h"

struct TaskReturn {
    uint64_t result;
    uint64_t val;
};

void syscall_handler(TaskDescriptor* task);

uint64_t get_page(uint64_t virtual_addr);
uint64_t release_page(uint64_t virtual_addr);
TaskReturn getpid(TaskDescriptor*);
TaskReturn syscall_create_process();