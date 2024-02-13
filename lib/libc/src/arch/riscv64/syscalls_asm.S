    .text
    .globl pmos_syscall
    .type pmos_syscall, @function
pmos_syscall:
    # Linux sets syscall number in a7
    # Don't overcomplicate things and pass the number in a0, as the
    # function is called.
    # Might be changed later.
    ecall
    ret