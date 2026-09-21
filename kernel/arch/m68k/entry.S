    .equ STACK_SIZE, 0x4000

    .bss
    .align 2
    .comm stack, STACK_SIZE

    .text
    .globl _limine_entry
    .type _limine_entry, @function
_limine_entry:
    lea stack(%pc), %sp
    add.l #STACK_SIZE, %sp

    jsr limine_main

    jmp return_from_kernel

    // TODO!!!
return_from_kernel: