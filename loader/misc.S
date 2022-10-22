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

ENTRY(clear_page)
    movq $512, %rcx
clear_page_start:
    movq $0, -8(%rdi, %rcx, 8)
    loop clear_page_start
    ret
