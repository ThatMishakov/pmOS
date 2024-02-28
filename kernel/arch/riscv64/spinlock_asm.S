    .text
    .globl acquire_lock_spin
    .type acquire_lock_spin, @function
acquire_lock_spin:
    li t0, 1
loop:
    lw t1, (a0)
    bnez t1, loop
    amoswap.w.aq t1, t0, (a0)
    bnez t1, loop
    ret

    .globl release_lock
    .type release_lock, @function
release_lock:
    amoswap.w.rl zero, zero, (a0)
    ret

    .globl try_lock
    .type try_lock, @function
try_lock:
    li t0, 1
    lw t1, (a0)
    beqz t1, fail
    amoswap.w.aq t1, t0, (a0)
    beqz t1, fail
    li a0, 1
    ret
fail:
    li a0, 0
    ret