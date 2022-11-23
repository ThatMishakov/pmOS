    .section .text
    .globl _start
_start:
    # Set up stack frame
    movq $0, %rbp
	pushq %rbp
	pushq %rbp
	movq %rsp, %rbp

    # xchgw %bx, %bx

    # Save arguments
    pushq %rdi
    pushq %rsi

    call init_std_lib

    call _init

    popq %rdi
    popq %rsi

    call prepare_args

    movq %rax, %rdi
    movq %rdx, %rsi

    call main

    pushq %rax
    call _fini
    popq %rax

    movq %rax, %rdi
    call exit
    .size _start, . - _start