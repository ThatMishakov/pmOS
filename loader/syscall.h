#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

typedef struct {
    uint64_t result;
    uint64_t value;
} syscall_r;

syscall_r syscall(int64_t call_n, ...);

#endif