.equ STACK_SIZE, 0x4000

.bss
.align 4
.comm stack, STACK_SIZE, 16

    .text
    .globl _limine_entry
    .type _limine_entry, %function
_limine_entry:
    msr spsel, #1
    adrp x0, stack + STACK_SIZE
    add  x0, x0, :lo12:stack + STACK_SIZE
    mov  sp, x0

    bl limine_main

    mrs x0, tpidr_el1
    ldr x0, [x0, #8]
    msr spsel, #1
    mov  sp, x0
    b return_to_user
