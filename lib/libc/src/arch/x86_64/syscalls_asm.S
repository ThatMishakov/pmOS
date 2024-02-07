    .text
    .globl pmos_syscall
    .type pmos_syscall, @function
pmos_syscall:
    # ----- Software int -------
    # int $0xca

    # ----- syscall -------
    movq %rcx, %r10
    syscall

    # ------ sysenter -------
    # movq %rcx, %r10
    # movq %rdx, %r11
    # leaq sysenter_ret(%rip), %rdx
    # movq %rsp, %rcx

    # sysenter
# sysenter_ret:
    # movq %r11, %rdx

    ret