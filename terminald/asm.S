    .globl bochs_out
    .type bochs_out, function
bochs_out:
    movb %dil, %al
    outb $0xe9
    ret