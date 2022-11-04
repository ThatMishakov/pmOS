    .section .text
    .globl _start
_start:
    # Set up stack frame
    movq $0, %rbp
	pushq %rbp
	pushq %rbp
	movq %rsp, %rbp

    call init_std_lib

    # call _init


    call main

    # call _fini

    movq %rax, %rdi
    call exit
    .size _start, . - _start