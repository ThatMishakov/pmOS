#pragma once
#include "sched.hh"
#include "common/errors.h"
#include "common/syscalls.h"

void syscall_handler(Interrupt_Register_Frame* regs);

uint64_t get_page(Interrupt_Register_Frame* regs);