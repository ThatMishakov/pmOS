    .text
    .type fast_copy_from_user, @function
    .globl fast_copy_from_user
fast_copy_from_user:    
    li   t0, 1
    j generic

    .type fast_copy_to_user, @function
    .globl fast_copy_to_user
fast_copy_to_user:
    li   t0, 2

generic:
    addi sp, sp, -144
    sd t0, 24(sp)
    sd fp, 128(sp)
    sd ra, 136(sp)
    addi fp, sp, 144

    # Arguments
    sd a0, 0(sp) # dst
    sd a1, 8(sp) # src
    sd a2, 16(sp) # size

    li t0, 0x40000
    csrs sstatus, t0


    sd s0, 32(sp)
    sd s1, 40(sp)
    sd s2, 48(sp)
    sd s3, 56(sp)
    sd s4, 64(sp)
    sd s5, 72(sp)
    sd s6, 80(sp)
    sd s7, 88(sp)
    sd s8, 96(sp)
    sd s9, 104(sp)
    sd s10, 112(sp)
    sd s11, 120(sp)

    la t0, on_failure
    sd t0, 48(tp)
    sd sp, 56(tp)

    call memcpy

    li t0, 0x40000
    csrc sstatus, t0

    sd zero, 48(tp)
    sd zero, 56(tp)

    # Success
    xor a0, a0, a0
    li  a1, 1

    ld  fp, 128(sp)
    ld  ra, 136(sp)
    addi sp, sp, 144
    ret

on_failure:
    mv sp, a0
    addi fp, sp, 144

    # Pass arguments
    addi a4, sp, 16
    addi a3, sp, 8
    addi a2, sp, 0
    # faulting addr is already in a1
    ld a0, 24(sp) # unsigned access

    call user_access_page_fault

    bne a0, zero, failure
    beq a1, zero, failure

    ld a0, 0(sp)
    ld a1, 8(sp)
    ld a2, 16(sp)

    call memcpy

    xor a0, a0, a0
    li a1, 1

failure:
    li t0, 0x40000
    csrc sstatus, t0

    sd zero, 48(tp)
    sd zero, 56(tp)

    ld s0, 32(sp)
    ld s1, 40(sp)
    ld s2, 48(sp)
    ld s3, 56(sp)
    ld s4, 64(sp)
    ld s5, 72(sp)
    ld s6, 80(sp)
    ld s7, 88(sp)
    ld s8, 96(sp)
    ld s9, 104(sp)
    ld s10, 112(sp)
    ld s11, 120(sp)

    ld fp, 128(sp)
    ld ra, 136(sp)
    addi sp, sp, 144

    ret