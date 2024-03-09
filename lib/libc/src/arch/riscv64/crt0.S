    .section .text
    .globl _start
_start:
    # Set up frame pointer
    # RISC-V Calling convention says it's optional, but it doesn't
    # hurt to include it
    #
    # This can be used to make GCC generate frame pointers:
    # riscv64-linux-gnu-g++ -O1 -fno-omit-frame-pointer -S test.cc
    addi sp, sp, -16
    # Since this is the first frame, set everything to 0
    sd zero, 0(sp) # Previous frame pointer
    sd zero, 8(sp) # Return address
    # Unlike x86, this points to the top of the stack frame
    addi fp, sp, 16
    # Also, stack needs to be 16-byte aligned, but it already is

    # Load global pointer
    # https://five-embeddev.com/quickref/global_pointer.html
    .option push
	.option norelax
	la gp, __global_pointer$
	.option pop

    # Initialize libc
    call init_std_lib

    # C++ (or others) constructors
    call __call_init_functions

    # Get argc and argv
    call prepare_args
    # This function returns struct { long argc; char **argv; };
    # in a0 and a1, as per RISC-V calling convention.
    # Conveniently, these are also the first function arguments
    # Thus, main should recieve those with no register moving
    # like on x86
    call main

    # Call exit
    # The return value of main is also the first argument to function
    # calls.
    call exit

    # This should never be reached
    # UNIMP should always cause an illegal instruction exception
    unimp