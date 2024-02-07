    .section .text
    .globl __get_tls
__get_tls:
    mv a0, tp
    ret