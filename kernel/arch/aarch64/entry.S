.equ STACK_SIZE, 0x4000

.bss
.align 4
.comm stack, STACK_SIZE

    .text
    .globl _limine_entry
    .type _limine_entry, %function
_limine_entry:
    adr x0, stack + STACK_SIZE
    mov sp, x0

    bl limine_main

    b return_to_user