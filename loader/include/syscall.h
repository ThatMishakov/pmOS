#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include "../../kernel/common/syscalls.h"

typedef struct {
    uint64_t result;
    uint64_t value;
} syscall_r;

syscall_r syscall(int64_t call_n, ...);

inline syscall_r get_page(uint64_t addr)
{
    return syscall(SYSCALL_GET_PAGE, addr);
}

#endif