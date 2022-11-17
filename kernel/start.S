#define STACK_SIZE 4096

    .globl _start
    .type _start, @function
_start:
    // Switch to temp in-kernel stack
    movq %rsp, stack_end-8(%rip)
    leaq (stack_end-8)(%rip), %rsp

    call main

    // Return to loader
    popq %rsp
    ret


    .globl cpu_startup_entry
    .type cpu_startup_entry, @function
cpu_startup_entry:
    leaq (stack_end)(%rip), %rsp # Load temp kernel stack

    movl $1, %edx
spin:
    xorl %eax, %eax
    lock cmpxchgl %edx, init_lock(%rip)
    jnz spin

    call cpu_start_routine

    movl $0, init_lock(%rip)

    movq %gs:24, %r15
    movq %r15, %gs:16
    movq $0, %gs:24

    movq %gs:8, %rsp
    addq $4096, %rsp

    jmp start_first_process

    

    .bss
    .comm stack, STACK_SIZE
stack_end:

    .data
init_lock:
    .long 0