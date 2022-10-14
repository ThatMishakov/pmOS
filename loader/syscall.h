#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

int64_t syscall(int64_t call_n, ...);

#endif