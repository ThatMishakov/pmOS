    .globl swapcontext
    .type swapcontext, @function
swapcontext:
    # Build the stack frame
    addi sp, sp, -32
    sd   ra, 24(sp)
    sd   s0, 16(sp)
    addi fp, sp, 32

    sd   a0, 0(sp) # Save the ucp
    sd   a1, 8(sp)

    # Verify that the ucp is valid
    beqz a0, .Linvalid_ucp
    beqz a1, .Linvalid_ucp

    # Save the sigmask
    addi a2, a0, 272 # &ucp->uc_sigmask
    li a0, 3         # SIG_SETMASK
    li a1, 0         # NULL
    call pthread_sigmask

    # Save the registers, stack pointer, and return address
    ld   t0,  0(sp)
    ld   t1,  16(sp)

    sd   t1,  64(t0)
    sd   s1,  72(t0)
    sd   s2,  144(t0)
    sd   s3,  152(t0)
    sd   s4,  160(t0)
    sd   s5,  168(t0)
    sd   s6,  176(t0)
    sd   s7,  184(t0)
    sd   s8,  192(t0)
    sd   s9,  200(t0)
    sd   s10, 208(t0)
    sd   s11, 216(t0)
    sd   fp,  16(t0) # fp is pointing to the stored sp

    ld   ra,  24(sp)
    sd   ra,  8(t0)
    # Set mcontext.partial to 1
    li   t1, 1
    sw   t1, 256(t0)

    # Restore the context
    # This would only return in case of error
    ld   a0, 8(sp)
    call setcontext

    # Restore the frame pointer and return
    ld   fp, 16(sp)
    ld   ra, 24(sp)
    addi sp, sp, 32

    # The error is passed in a0
    ret

.Linvalid_ucp:
    call __get_errno
    li   t0, 31 # EINVAL
    sw   t0, 0(a0)
    li   a0, -1

    ld fp, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32

    ret

