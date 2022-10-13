    .globl printc
    .type printc, @function
printc:
    movb %dil, %al
    outb $0xe9
    ret

    .globl tlb_flush
    .type tlb_flush, @function
tlb_flush:
    movq %cr3, %rax
    movq %rax, %cr3
    ret

    .globl page_clear
    .type page_clear, @function
page_clear:
    movq $512, %rcx
.start_c:
    movq $0, -8(%rdi, %rcx, 8);
    loop .start_c
    retq

   .globl memset
    .type memset, @function
memset:
    movq %rsi, %rcx
.start:
    movq $0, -8(%rdi, %rcx, 8);
    loop .start
    retq

    .globl loadTSS
    .type loadTSS, @function
loadTSS:
    movq %rdi, %rax
    ltr %ax
    ret
