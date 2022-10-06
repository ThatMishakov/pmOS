.text
    .globl loadGDT 
    .type loadGDT, @function
loadGDT:
    lgdt 4(%esp)
    ljmp   $0x08, $reload_CS
reload_CS:
    movw $0x10, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    ret