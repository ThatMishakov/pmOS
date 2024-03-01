    .text
    .type isr, @function
    .globl isr
isr:
    # Swap user (on user interrupts) thread pointer with the kernel thread pointer
    # If the interrupt is from kernel, they should be the same anyways
    csrrw tp, sscratch, tp

    # Save one temporary register and load the location where the registers
    # will be saved into it.
    # sched/sched.hh has the struct layout
    sd t0, 16(tp)

    # Load the address of the 
    # TODO: At the moment, in-kernel interrupts are not supported
    # Load task struct info
    ld t0, 24(tp)

    # Save registers
    sd x1, 8(t0)  # ra
    sd x2, 16(t0) # sp
    sd x3, 24(t0) # gp
    # tp. At the moment, it is in the sscratch register
    # t0 is in CPU struct at the moment
    sd x6, 48(t0) # t1
    sd x7, 56(t0) # t2
    sd x8, 64(t0) # s0
    sd x9, 72(t0) # s1
    sd x10, 80(t0) # a0
    sd x11, 88(t0) # a1
    sd x12, 96(t0) # a2
    sd x13, 104(t0) # a3
    sd x14, 112(t0) # a4
    sd x15, 120(t0) # a5
    sd x16, 128(t0) # a6
    sd x17, 136(t0) # a7
    sd x18, 144(t0) # s2
    sd x19, 152(t0) # s3
    sd x20, 160(t0) # s4
    sd x21, 168(t0) # s5
    sd x22, 176(t0) # s6
    sd x23, 184(t0) # s7
    sd x24, 192(t0) # s8
    sd x25, 200(t0) # s9
    sd x26, 208(t0) # s10
    sd x27, 216(t0) # s11
    sd x28, 224(t0) # t3
    sd x29, 232(t0) # t4
    sd x30, 240(t0) # t5
    sd x31, 248(t0) # t6

    # Save t0
    ld t1, 16(tp)
    sd t1, 40(tp)

    # Save tp
    # Read user tp
    csrr t1, sscratch
    # Save kernel ssratch
    csrw sscratch, tp
    # Save user tp
    sd t1, 32(t0)

    # Save program counter
    csrr t1, sepc
    sd t1, 0(t0)

    # Switch to kernel stack
    ld sp, 8(tp)

    # Set frame pointer for debugging and stuff
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)
    addi fp, sp, 16

    # Tick the nested_level counter, to know if the interrupt is in kernel next time
    ld t1, 40(tp)
    addi t1, t1, 1
    sd t1, 40(tp)



    # Jump to C++
    call handle_interrupt
    # Return from kernel



    # nested_level counter
    ld t1, 40(tp)
    addi t1, t1, -1
    sd t1, 40(tp)

    # Load the struct holding the saved registers
    ld t0, 24(tp)

    # SPP bit
    # If the task is in user space, clear it, otherwise set it
    li t1, 0x100 # SPP bit mask
    csrc sstatus, t1
    ld t2, 256(t0)
    beq t2, zero, 1f
    csrs sstatus, t1
1:

    # Restore program counter
    ld t1, 0(t0)
    csrw sepc, t1

    # Restore registers
    ld x31, 248(t0) # t6
    ld x30, 240(t0) # t5
    ld x29, 232(t0) # t4
    ld x28, 224(t0) # t3
    ld x27, 216(t0) # s11
    ld x26, 208(t0) # s10
    ld x25, 200(t0) # s9
    ld x24, 192(t0) # s8
    ld x23, 184(t0) # s7
    ld x22, 176(t0) # s6
    ld x21, 168(t0) # s5
    ld x20, 160(t0) # s4
    ld x19, 152(t0) # s3
    ld x18, 144(t0) # s2
    ld x17, 136(t0) # a7
    ld x16, 128(t0) # a6
    ld x15, 120(t0) # a5
    ld x14, 112(t0) # a4
    ld x13, 104(t0) # a3
    ld x12, 96(t0) # a2
    ld x11, 88(t0) # a1
    ld x10, 80(t0) # a0
    ld x9, 72(t0) # s1
    ld x8, 64(t0) # s0
    ld x7, 56(t0) # t2
    ld x6, 48(t0) # t1
    ld x4, 32(t0) # tp
    ld x3, 24(t0) # gp
    ld x2, 16(t0) # sp
    ld x1, 8(t0)  # ra

    # t0 has to go last
    ld x5, 40(t0) # t0

    # Return to *wherever*
    sret

