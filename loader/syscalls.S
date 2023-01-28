#include <kernel/syscalls.h>

    .globl syscall
    .type syscall, @function
syscall:
    movq %rcx, %r10
    syscall
    ret