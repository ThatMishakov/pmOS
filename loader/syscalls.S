#include "../kernel/common/syscalls.h"

    .globl syscall
    .type syscall, @function
syscall:
    int $PMOS_SYSCALL_INT
    ret