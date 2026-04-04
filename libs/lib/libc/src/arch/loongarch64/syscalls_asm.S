    .text
    .globl pmos_syscall
    .type pmos_syscall, @function
pmos_syscall:
    addi.d $sp, $sp, -16
    st.d $fp, $sp, 0
    st.d $ra, $sp, 8
    addi.d $fp, $sp, 16

    syscall 0

    ld.d $fp, $sp, 0
    addi.d $sp, $sp, 16
    jr $ra