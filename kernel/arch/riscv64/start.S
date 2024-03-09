.equ STACK_SIZE, 0x2000

.bss
.align 4
.comm stack, STACK_SIZE

.text
.globl _limine_entry
.type _limine_entry, @function
_limine_entry:
    # If the kernel was relocatable, gp would need to be set here

    # Switch to our own stack since the one provided by limine will be unusable
    # once the kernel switched to its own page tables
    la sp, stack
    li t0, STACK_SIZE
    add sp, sp, t0

    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)
    addi fp, sp, 16

    call limine_main

    # Return to userspace
    j return_from_kernel
