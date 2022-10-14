#pragma once
#include "sched.hh"

#define PMOS_SYSCALL_INT 0xca

void syscall_handler(Interrupt_Register_Frame* regs);