#include <asm.h>

ENTRY(printc)
    movb %dil, %al
    outb $0xe9
    ret
