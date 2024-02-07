    .section .text
    .globl __get_tls
__get_tls:
    movq %fs:0, %rax
    ret