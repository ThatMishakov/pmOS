    .type __setcontext_partial, @function
    .globl __setcontext_partial
__setcontext_partial:
    # Restore saved registers
    #ld s0, 64(a0)
    mv s0, a0
    ld s1, 72(a0)
    ld s2, 144(a0)
    ld s3, 152(a0)
    ld s4, 160(a0)
    ld s5, 168(a0)
    ld s6, 176(a0)
    ld s7, 184(a0)
    ld s8, 192(a0)
    ld s9, 200(a0)
    ld s10, 208(a0)
    ld s11, 216(a0)

    ld sp, 16(a0) # Stack pointer

    # Restore signal mask
    addi a1, a0, 272 # &ucp->uc_sigmask
    li a0, 3         # SIG_SETMASK
    li a2, 0         # NULL
    call pthread_sigmask

    # Return to the caller
    ld ra, 8(s0)
    ld s0, 64(s0)
    ret
