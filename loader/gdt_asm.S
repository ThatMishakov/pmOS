DW:
    .short
    .short
    .short

.text
    .globl loadGDT 
    .type loadGDT, @function
loadGDT:
    movw   8(%esp), %ax
    movw   %ax, DW
    movw   4(%esp), %ax
    movw   %ax, DW+2
    movw   6(%esp), %ax
    movw   %ax, DW+4
    lgdt   DW
    ljmp   $0x08, $reload_CS
reload_CS:
    movw $0x10, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    ret

