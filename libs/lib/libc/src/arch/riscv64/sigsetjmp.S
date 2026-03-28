    .text
    .globl sigsetjmp
    .type sigsetjmp, @function
sigsetjmp:
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)
    
    # Save the signal mask if the second argument is set
    bnez a1, 1f

    # Set restore_mask to 1
    li a3, 1
    sw a3, 112(a0)

    # TODO: Save the signal mask

    j 2f
1:
    # Set restore_mask to 0
    li a3, 0
2:
    # Set restore mask to the appropriate value
    sw a3, 112(a0)

    # Set the return value to 0
    li a0, 0
    ret
    .size sigsetjmp, .-sigsetjmp

    .globl siglongjmp
    .type siglongjmp, @function
siglongjmp:
    ld ra, 0(a0)
    ld sp, 8(a0)
    ld s0, 16(a0)
    ld s1, 24(a0)
    ld s2, 32(a0)
    ld s3, 40(a0)
    ld s4, 48(a0)
    ld s5, 56(a0)
    ld s6, 64(a0)
    ld s7, 72(a0)
    ld s8, 80(a0)
    ld s9, 88(a0)
    ld s10, 96(a0)
    ld s11, 104(a0)

    # Restore the mask here
    ld s3, 112(a0)
    bnez s3, 1f

    # TODO

1:
    # Pass the return value
    mv a0, a1

    # Return (to the new address)
    ret