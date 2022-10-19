    .globl _start
    .type _start, @function
_start:
    // Do initialization, call constructors


call main


    // Call destructors
    

    // Exit
    movq $0x0a, %rdi
    int $0xca
