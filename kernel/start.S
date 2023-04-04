#define STACK_SIZE 4096
    .include "interrupts/asm_macros.S"

    .globl _start
    .type _start, @function
_start:
    // Switch to temp in-kernel stack
    movq %rsp, (stack+STACK_SIZE-8)(%rip)
    leaq (stack+STACK_SIZE-16)(%rip), %rsp

    call main

    // Return to loader
    addq $8, %rsp
    popq %rsp

    pushf
    popq %r11
    popq %rcx
    swapgs
    sysretq

    # Entry point for bootstraping CPUs
    .globl cpu_startup_entry
    .type cpu_startup_entry, @function
cpu_startup_entry:
    leaq (stack+STACK_SIZE)(%rip), %rsp # Load temp kernel stack

    # Spinlock on initialization since we only have 1 stack...
    movl $1, %edx
spin:
    xorl %eax, %eax
    lock cmpxchgl %edx, init_lock(%rip)
    jnz spin

    # Jump to C++ initialization code
    call cpu_start_routine

    # Switch to the new stack
    call get_kern_stack_top
    movq %rax, %rsp

    # Unlock the spinlock
    movl $0, init_lock(%rip)

    # Jump to the userspace
    auto_return

    

    .bss
    .align 16
    .comm stack, STACK_SIZE

    .data
init_lock:
    .long 0