    .globl printc
    .type printc, @function
printc:
    movb %dil, %al
    outb $0xe9
    ret