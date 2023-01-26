#include <kernel/syscalls.h>

    .globl syscall
    .type syscall, @function
syscall:
    movq %rcx, %r10
    int $PMOS_SYSCALL_INT
    ret