#include <asm.h>

ENTRY(printc)
    movb %dil, %al
    outb $0xe9
    ret

ENTRY(tlb_flush)
    movq %cr3, %rax
    movq %rax, %cr3
    ret

ENTRY(halt)
    hlt
    jmp halt
