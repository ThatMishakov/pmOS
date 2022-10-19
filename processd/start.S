    .globl _start
    .type _start, @function
_start:
    // Do initialization, call constructors


call main


    // Call destructors
    // Return to kernel

    // block
    movq $0x02, %rdi
    int $0xca
