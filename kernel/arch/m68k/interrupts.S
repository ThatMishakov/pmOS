    .macro save_regs
    move.l (%sp)+, (%a0) | Save d0
    movem.l %d1-%d7, 4(%a0) | Save d1-d7
    move.l (%sp)+, 32(%a0) | Save a0
    movem.l %a1-%a6, 36(%a0) | Save a1-a6


    | PC and sr for convenience...
    move.w (%sp), 68(%a0)
    move.l 2(%sp), 64(%a0)

    lea 76(%a0), %a1

    move.w 6(%sp), %d0
    rol.w #4, %d0
    andi.w #0x000f, %d0
    move.l %pc@(frame_sizes_words, %d0:w*4), %d1

    subq.l #1, %d1

1:
    move.w (%a7)+, (%a1)+
    dbra %d1, 1b
    .endm

    .text
    .globl interrupt_entry
    .type interrupt_entry, @function
interrupt_entry:
    move.l %a0, -(%sp)
    move.l %d0, -(%sp)

    lea cpu_struct(%pc), %a0
    addi.l #1, 12(%a0)  | Increment nested_int_count

    cmp.l #1, 12(%a0)
    bne kernel_exception

    move.w 8(%sp), %d0
    btst.l #13, %d0
    bne userspace_task

    and.w  #0x700, %d0
    bne kernel_exception | Preempting another interrupt

    lea.l 16(%a0), %a0 | Current task

    move.l %sp, 60(%a0)
    
    save_regs

    move.l cpu_struct+4(%pc), %sp

    bsr user_exception_handler

    jmp return_from_user

userspace_task:
    lea.l 16(%a0), %a0 | Current task

    movec   %usp, %a1
    move.l %a1, 60(%a0) | Save user stack pointer

    save_regs

    bsr user_exception_handler

return_from_user:

    move.l 16(%pc), %a0 | Current task
    btst.l #13, 68(%a0)
    beq 1f

    move.l 60(%a0), %a1
    movec   %a1, %usp
    jmp 2f

1:
    move.l 60(%a0), %sp
2:

    ori.w #0x0700, %sr
    lea cpu_struct(%pc), %a2
    subi.l #1, 12(%a2)  | Decrement nested_int_count

    move.w 82(%a0), %d0
    rol.w #4, %d0
    andi.w #0x000f, %d0

    move.l %pc@(frame_sizes_words, %d0:w*4), %d0

    move.l %d0, %d1
    add.l %d1, %d1
    sub.l %d1, %sp

    subq.l #1, %d0

1:
    move.w %a0@(76,%d0:l*2), %sp@(0,%d0:l*2)
    dbra %d0, 1b

    move.l 64(%a0), 2(%sp)
    move.w 68(%a0), (%sp)

    movem.l (%a0), %d0-%d7/%a0-%a6

    rte

kernel_exception:

    | TODO!



