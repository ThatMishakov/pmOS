#include "sbi/sbi.hh"

void printc(int c) {
    sbi_debug_console_write_byte(c);
}

extern "C" void dbg_uart_putc(int c) {
    printc(c);
}