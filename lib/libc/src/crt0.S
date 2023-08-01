    .section .text
    .globl _start
_start:
    # Set up stack frame
    movq $0, %rbp
	pushq %rbp
	pushq %rbp
	movq %rsp, %rbp

    # Save arguments
    pushq %rdi
    pushq %rsi

    movq %rdx, %rdi
    call init_tls_first_time

    call init_std_lib

    call _init

    popq %rsi
    popq %rdi
    
    call prepare_args

    movq %rax, %rdi
    movq %rdx, %rsi

    call main

    movq %rax, %rdi
    call exit
    .size _start, . - _start