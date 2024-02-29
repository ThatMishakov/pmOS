    .text

    .globl sbi_debug_console_write_byte
    .type sbi_debug_console_write_byte, @function
sbi_debug_console_write_byte:
    li a6, 1
    li a7, 0x4442434E
    ecall
    ret

    .globl sbi_set_timer
    .type sbi_set_timer, @function
sbi_set_timer:
    li a6, 0
    li a7, 0x54494D45
    ecall
    ret