.equ STACK_SIZE, 0x4000

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
    la $sp, stack
    li.d $t0, STACK_SIZE
    add.d $sp, $sp, $t0

    addi.d $sp, $sp, -16
    st.d $zero, $sp, 0
    st.d $zero, $sp, 8
    addi.d $fp, $sp, 16

    bl limine_main

    # Return to userspace
    b return_from_kernel