    .section .text
    .globl _start
_start:
    # Set up stack frame
    movq $0, %rbp
	pushq %rbp

    # Initialize the libc
    call init_std_lib
    
    # C++ (or others) constructors
    call __call_init_functions
    
    # Get argc and argv
    call prepare_args

    movq %rax, %rdi
    movq %rdx, %rsi

    call main

    # This is equivalent to calling exit from main()
    movq %rax, %rdi
    call exit
    .size _start, . - _start