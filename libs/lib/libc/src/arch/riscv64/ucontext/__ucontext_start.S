    .type __ucontext_start, @function
    .globl __ucontext_start
    .hidden __ucontext_start
__ucontext_start:
    # Load the parameters from saved registers
    mv a0, s2
    mv a1, s3
    mv a2, s4
    mv a3, s5
    mv a4, s6
    mv a5, s7
    mv a6, s8
    mv a7, s9
    # Other parameters are on the stack

    # Call the function
    jalr s10

    # Cleanup
    mv a0, s1
    call __ucontext_cleanup

    unimp