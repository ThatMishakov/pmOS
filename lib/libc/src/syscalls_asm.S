#include <kernel/syscalls.h>

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

    .globl _syscall_exit
    .type _syscall_exit, @function
_syscall_exit:
    pushq %rbp
    movq %rsp, %rbp
    movq %rdi, %rsi
    movq $SYSCALL_EXIT, %rdi
    call pmos_syscall
    popq %rbp
    ret