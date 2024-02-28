.equ STACK_SIZE, 0x1000

.bss
.align 16
.comm stack, STACK_SIZE

.text
.globl _limine_entry
.type _limine_entry, @function
_limine_entry:
    # If the kernel was relocatable, gp would need to be set here

    # Switch to our own stack since the one provided by limine will be unusable
    # once the kernel switched to its own page tables
    la sp, stack
    addi sp, sp, %lo(STACK_SIZE)

    mv fp, zero
    mv ra, zero


    call limine_main

    # Halt the CPU if limine_main returns
    wfi
    j .
