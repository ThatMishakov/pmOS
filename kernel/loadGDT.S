    .globl loadGDT
    .type loadGDT, @function
loadGDT:
    pushq %rdi
    shlq $48, %rsi
    pushq %rsi
    leaq 6(%rsp), %rax
    lgdt (%rax)
    addq $16,%rsp

    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw $0x10, %ax
    movw %ax, %ss
    popq %rdi
    pushq $0x08
    pushq %rdi
    lretq

