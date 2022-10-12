    .globl loadGDT
    .type loadGDT, @function
loadGDT:
    cli
    lgdt (%rdi)
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

