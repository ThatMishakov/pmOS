    .text
    .type fast_copy_to_user, @function
    .globl fast_copy_to_user
fast_copy_to_user:
    addi.d $sp, $sp, -128
    li.d   $t0, 2
    b generic

    .type fast_copy_from_user, @function
    .globl fast_copy_from_user
fast_copy_from_user:
    addi.d $sp, $sp, -128
    li.d   $t0, 1

generic:
    st.d   $t0, $sp, 24


    st.d   $fp, $sp, 112
    st.d   $ra, $sp, 120
    addi.d $fp, $sp, 128

    # This might get destroyed by the memcpy shenanigans
    st.d   $s0, $sp, 32
    st.d   $s1, $sp, 40
    st.d   $s2, $sp, 48
    st.d   $s3, $sp, 56
    st.d   $s4, $sp, 64
    st.d   $s5, $sp, 72
    st.d   $s6, $sp, 80
    st.d   $s7, $sp, 88
    st.d   $s8, $sp, 96

    la.local $t0, on_failure
    st.d   $t0, $tp, 48 # CPU_Info::jumpto_func
    st.d   $sp, $tp, 56 # CPU_Info::jumpto_arg

    # Arguments...
    st.d   $a0, $sp, 0 # void *dst
    st.d   $a1, $sp, 8 # const void *src
    st.d   $a2, $sp, 16 # size_t size

    bl memcpy

    # Success
    xor    $a0, $a0, $a0
    li.d   $a1, 1

    ld.d   $ra, $sp, 120
    ld.d   $fp, $sp, 112
    addi.d $sp, $sp, 128
    ret

on_failure:
    move   $sp, $a0
    addi.d $fp, $sp, 128

    # Pass arguments
    addi.d $a4, $sp, 16 # size_t &size
    addi.d $a3, $sp, 8  # char *&src
    addi.d $a2, $sp, 0  # char *&dst
    # move   $a4, $a1     # const void *faulting_addr
    ld.d   $a0, $sp, 24 # unsigned access

    bl user_access_page_fault

    bne    $a0, $zero, failure
    beq    $a1, $zero, failure

    ld.d   $a0, $sp, 0 # void *dst
    ld.d   $a1, $sp, 8 # const void *src
    ld.d   $a2, $sp, 16 # size_t size

    bl memcpy

    xor   $a0, $a0, $a0
    li.d  $a1, 1

failure: # failure is a bit misleading, but I couldn't come up with a better label name :=P
    st.d   $zero, $tp, 48 # CPU_Info::jumpto_func
    st.d   $zero, $tp, 56 # CPU_Info::jumpto_arg

    ld.d   $s0, $sp, 32
    ld.d   $s1, $sp, 40
    ld.d   $s2, $sp, 48
    ld.d   $s3, $sp, 56
    ld.d   $s4, $sp, 64
    ld.d   $s5, $sp, 72
    ld.d   $s6, $sp, 80
    ld.d   $s7, $sp, 88
    ld.d   $s8, $sp, 96

    ld.d   $fp, $sp, 112
    ld.d   $ra, $sp, 120
    addi.d $sp, $sp, 128

    ret