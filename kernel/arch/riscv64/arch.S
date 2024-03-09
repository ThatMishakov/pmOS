    .text
    .globl allow_access_user
    .type allow_access_user, @function
allow_access_user:
    li t0, 0x40000 # SUM bit
    csrs sstatus, t0
    ret

    .globl disallow_access_user
    .type disallow_access_user, @function
disallow_access_user:
    li t0, 0x40000
    csrc sstatus, t0
    ret