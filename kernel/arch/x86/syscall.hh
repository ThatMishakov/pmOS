#pragma once

namespace kernel::x86 {

// Entry point for when userspace calls SYSCALL instruction
extern "C" void syscall_entry();

// Entry point for when userspace calls SYSENTER instruction
extern "C" void sysenter_entry();

// Enables SYSCALL instruction
void program_syscall();

}