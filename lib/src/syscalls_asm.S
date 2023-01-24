#include <kernel/syscalls.h>

    .globl syscall
    .type syscall, @function
syscall:
    pushq %rbp
    movq %rsp, %rbp
    movq %rcx, %rax

    syscall

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