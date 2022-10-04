#include <asm.h>

ENTRY(printc)
    movb 4(%esp), %al
    outb $0xe9
    ret
