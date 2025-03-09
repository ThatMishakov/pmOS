    .text
    .globl __do_fork
    .type __do_fork, @function
__do_fork:
    addi.d $sp, $sp, -160
    st.d $ra, $sp, 152
    st.d $fp, $sp, 144
    addi.d $fp, $sp, 160

    st.d $s0, $sp, 0
    st.d $s1, $sp, 8
    st.d $s2, $sp, 16
    st.d $s3, $sp, 24
    st.d $s4, $sp, 32
    st.d $s5, $sp, 40
    st.d $s6, $sp, 48
    st.d $s7, $sp, 56
    st.d $s8, $sp, 64
    # st.d $s9, $sp, 72

    fst.d $fs0, $sp, 80
    fst.d $fs1, $sp, 88
    fst.d $fs2, $sp, 96
    fst.d $fs3, $sp, 104
    fst.d $fs4, $sp, 112
    fst.d $fs5, $sp, 120
    fst.d $fs6, $sp, 128
    fst.d $fs7, $sp, 136

    # Pass the stack pointer as the third argument to C
    move $a2, $sp

    # Call inner fork function
    bl __libc_fork_inner

    # The saved registers are only needed for the child process
    ld.d $ra, $sp, 152
    ld.d $fp, $sp, 144
    addi.d $sp, $sp, 160

    jr $ra

    .globl __fork_child_entry_point
    .type __fork_child_entry_point, @function
__fork_child_entry_point:
    move $sp, $a0
    addi.d $fp, $sp, -160

    ld.d $s0, $sp, 0
    ld.d $s1, $sp, 8
    ld.d $s2, $sp, 16
    ld.d $s3, $sp, 24
    ld.d $s4, $sp, 32
    ld.d $s5, $sp, 40
    ld.d $s6, $sp, 48
    ld.d $s7, $sp, 56
    ld.d $s8, $sp, 64
    # st.d $s9, $sp, 72

    fld.d $fs0, $sp, 80
    fld.d $fs1, $sp, 88
    fld.d $fs2, $sp, 96
    fld.d $fs3, $sp, 104
    fld.d $fs4, $sp, 112
    fld.d $fs5, $sp, 120
    fld.d $fs6, $sp, 128
    fld.d $fs7, $sp, 136

    bl __libc_fork_child

    ld.d $ra, $sp, 152
    ld.d $fp, $sp, 144
    addi.d $sp, $sp, 160

    jr $ra
