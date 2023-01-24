#include <kernel/syscalls.h>

    .globl syscall
    .type syscall, @function
syscall:
    movq %rcx, %rax
    # int $0xca
    syscall
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