    .text
    .globl __do_fork
    .type __do_fork, @function
__do_fork:
    addi sp, sp, -112
    sd ra, 104(sp)
    sd fp, 96(sp)
    addi fp, sp, 112

    # Save saved registers
    sd s0, 0(sp)
    sd s1, 8(sp)
    sd s2, 16(sp)
    sd s3, 24(sp)
    sd s4, 32(sp)
    sd s5, 40(sp)
    sd s6, 48(sp)
    sd s7, 56(sp)
    sd s8, 64(sp)
    sd s9, 72(sp)
    sd s10, 80(sp)
    sd s11, 88(sp)

    # Pass the stack pointer as the third argument to C
    mv a2, sp

    # Call inner fork function
    call __libc_fork_inner

    # The saved registers are only needed for the child process
    ld ra, 104(sp)
    ld s0, 96(sp)
    addi sp, sp, 112

    # Return from the function
    ret

    .globl __fork_child_entry_point
    .type __fork_child_entry_point, @function
__fork_child_entry_point:
    mv sp, a0
    addi fp, sp, -112

    # Restore saved registers
    ld s0, 0(sp)
    ld s1, 8(sp)
    ld s2, 16(sp)
    ld s3, 24(sp)
    ld s4, 32(sp)
    ld s5, 40(sp)
    ld s6, 48(sp)
    ld s7, 56(sp)
    ld s8, 64(sp)
    ld s9, 72(sp)
    ld s10, 80(sp)
    ld s11, 88(sp)

    # Set global pointer
    la gp, __global_pointer$

    # Call the C function
    call __libc_fork_child

    # Exit the child process
    ld ra, 104(sp)
    ld s0, 96(sp)
    addi sp, sp, 112

    ret
