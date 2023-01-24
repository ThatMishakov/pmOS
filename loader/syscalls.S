#include <kernel/syscalls.h>

    .globl syscall
    .type syscall, @function
syscall:
    movq %rcx, %rax
    int $PMOS_SYSCALL_INT
    ret