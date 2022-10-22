    .globl loadGDT
    .type loadGDT, @function
loadGDT:
    pushq %rdi
    shlq $48, %rsi
    pushq %rsi
    leaq 6(%rsp), %rax
    lgdt (%rax)
    addq $16,%rsp

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    pop %rdi
    pushq $0x08
    pushq %rdi
    lretq

