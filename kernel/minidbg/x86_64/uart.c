/*
 * x86_64/uart.c
 *
 * Copyright (C) 2021 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief: IBM PC compatible UART functions for the mini debugger
 */

void dbg_printf(char *fmt, ...);


volatile int inited_uart = 0;

/**
 * Set baud rate and characteristics (115200 8N1)
 */
void dbg_uart_init()
{
    if (inited_uart || __sync_val_compare_and_swap(&inited_uart, 0, 1))
        return;

    __asm__ __volatile__(
        "movw $0x3f9, %%dx;"
        "xorb %%al, %%al;outb %%al, %%dx;"               /* IER int off */
        "movb $0x80, %%al;addb $2,%%dl;outb %%al, %%dx;" /* LCR set divisor mode */
        "movb $1, %%al;subb $3,%%dl;outb %%al, %%dx;"    /* DLL divisor lo 115200 */
        "xorb %%al, %%al;incb %%dl;outb %%al, %%dx;"     /* DLH divisor hi */
        "incb %%dl;outb %%al, %%dx;"                     /* FCR fifo off */
        "movb $0x43, %%al;incb %%dl;outb %%al, %%dx;"    /* LCR 8N1, break on */
        "movb $0x8, %%al;incb %%dl;outb %%al, %%dx;"     /* MCR Aux out 2 */
        "xorb %%al, %%al;subb $4,%%dl;inb %%dx, %%al"    /* clear receiver/transmitter */
    :::"rax","rdx");
}

/**
 * Send a character
 */
void dbg_uart_putc(unsigned int c)
{
    __asm__ __volatile__(
        "movl $10000,%%ecx;movw $0x3fd,%%dx;"
        "1:inb %%dx, %%al;pause;"
        "cmpb $0xff,%%al;je 2f;"
        "dec %%ecx;jz 2f;"
        "andb $0x20,%%al;jz 1b;"
        "subb $5,%%dl;movb %%bl, %%al;outb %%al, %%dx;2:"
    ::"b"(c):"rax","rcx","rdx");
}

/**
 * Receive a character
 */
char dbg_uart_getc() {
    char r;
    __asm__ __volatile__(
        "movw $0x3fd, %%dx;"
        "1: pause; inb %%dx, %%al;"
        "andb $1, %%al;"
        "jz 1b;"
        "subb $5, %%dl;"
        "inb %%dx, %%al;"
    :"=a"(r)::"rdx");
    return r=='\r'?'\n':r;
}

