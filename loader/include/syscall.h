#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include <kernel/syscalls.h>
#include <kernel/memory.h>
#include <pmos/system.h>

syscall_r syscall(uint64_t call_n, ...);

inline syscall_r start_process(uint64_t pid, uint64_t entry, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    return syscall(SYSCALL_START_PROCESS, pid, entry, arg1, arg2, arg3);
}

#endif