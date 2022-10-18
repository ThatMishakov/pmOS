#define STACK_SIZE 4096

    .globl _start
    .type _start, @function
_start:
    // Switch to temp in-kernel stack
    movq %rsp, stack+STACK_SIZE-8(%rip)
    leaq (stack+STACK_SIZE-8)(%rip), %rsp

    call main

    // Return to loader
    popq %rsp
    ret

    .bss
    .comm stack, STACK_SIZE