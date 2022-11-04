#include "../../kernel/common/syscalls.h"

    .globl syscall
    .type syscall, @function
syscall:
    pushq %rbp
    movq %rsp, %rbp
    int $PMOS_SYSCALL_INT
    popq %rbp
    ret

    .globl exit
    .type exit, @function
exit:
    pushq %rbp
    movq %rsp, %rbp
    movq %rdi, %rsi
    movq $SYSCALL_EXIT, %rdi
    call syscall
    popq %rbp
    ret