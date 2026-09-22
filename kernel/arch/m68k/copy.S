    .text
    .even
    .globl  fetch_user_u32
    .type   fetch_user_u32,@function

fetch_user_u32:
    move.l  8(%sp),%a1          | a1 = user addr
    move.l  4(%sp),%a0          | a0 = destination addr
    move.l  %d2,-(%sp)          | save scratch reg (callee-saved)

    movec   %sfc,%d2            | d2 = caller's SFC, to restore
    moveq   #1,%d1
    movec   %d1,%sfc            | SFC = 1 (user data space)

    moves.l (%a1),%d0           | atomic 32-bit read from user space
    move.l  %d0,(%a0)           | store to destination

    movec   %d2,%sfc            | restore SFC
    move.l  (%sp)+,%d2          | restore d2
    rts

    .size   fetch_user_u32,.-fetch_user_u32



    .text
    .even
    .globl   fast_atomic_read_from_user
    .type    fast_atomic_read_from_user,@function
fast_atomic_read_from_user:
    link    %fp, #48
    move.l  %a5, -4(%fp)
    move.l  %a4, -8(%fp)
    move.l  %a3, -12(%fp)
    move.l  %a2, -16(%fp)
    move.l  %d7, -20(%fp)
    move.l  %d6, -24(%fp)
    move.l  %d5, -28(%fp)
    move.l  %d4, -32(%fp)
    move.l  %d3, -36(%fp)
    move.l  %d2, -40(%fp)

    | Save the SFC register
    movec   %sfc,%d0
    move.l   %d0, -44(%fp)

    | Return value
    move.l  %a0, -48(%fp)

    | Set the trampoline
    lea     cpu_struct(%pc), %a0
    move.l  %sp, 28(%a0)
    lea fast_read_failure(%pc), %a1
    move.l  %a1, 24(%a0)

    move.l  12(%fp), -(%sp)
    move.l  8(%fp), -(%sp)
    jsr fetch_user_u32

    | Success
    lea     cpu_struct(%pc), %a0
    move.l  #0, 24(%a0)

    move.l -48(%fp), %a0
    move.l  #0, 0(%a0)
    move.l  #0, 4(%a0)
    move.b  #0, 8(%a0)

    unlk  %fp
    rts

fast_read_failure:
    move.l %a0, %sp
    lea 48(%sp), %fp

    | Args
    move.l #0, -(%sp)        | size_t *size = nullptr
    move.l #0, -(%sp)        | void **src = nullptr
    move.l #0, -(%sp)        | void **dst = nullptr
    move.l %a1, -(%sp)       | const void *faultung_addr
    move.l #1, -(%sp)        | access_type = 1 (read)
    move.l -48(%fp), %a0     | a0 = ReturnStr<bool> *return_value
    jsr user_access_page_fault
    add.l #20, %sp

    tst.l  0(%a0)
    bne common_return
    tst.l  4(%a0)
    bne common_return
    cmpi.b  #1, 8(%a0)
    bne common_return

    move.l  12(%fp), -(%sp)
    move.l  8(%fp), -(%sp)
    jsr fetch_user_u32
    | Don't bother with poping stack here

common_return:
    lea     cpu_struct(%pc), %a0
    move.l  #0, 24(%a0)

    move.l 4(%fp), %d0
    movec   %d0,%sfc

    move.l  -48(%fp), %a0
    
    move.l -40(%fp), %d2
    move.l -36(%fp), %d3
    move.l -32(%fp), %d4
    move.l -28(%fp), %d5
    move.l -24(%fp), %d6
    move.l -20(%fp), %d7
    move.l -16(%fp), %a2
    move.l -12(%fp), %a3
    move.l -8(%fp), %a4
    move.l -4(%fp), %a5
    unlk  %fp
    rts
