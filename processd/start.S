    .globl _start
    .type _start, @function
_start:
    // Do initialization, call constructors


call main


    // Call destructors
    // Return to kernel
start:
    jmp start
