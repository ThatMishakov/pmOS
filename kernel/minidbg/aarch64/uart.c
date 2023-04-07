/*
 * aarch64/uart.c
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
 * @brief: Raspberry Pi UART functions for the mini debugger
 */

#define GPFSEL1         ((volatile unsigned int*)(MMIO_BASE+0x00200004))
#define GPPUD           ((volatile unsigned int*)(MMIO_BASE+0x00200094))
#define GPPUDCLK0       ((volatile unsigned int*)(MMIO_BASE+0x00200098))

/* PL011 UART registers */
#define UART0_DR        ((volatile unsigned int*)(MMIO_BASE+0x00201000))
#define UART0_FR        ((volatile unsigned int*)(MMIO_BASE+0x00201018))
#define UART0_IBRD      ((volatile unsigned int*)(MMIO_BASE+0x00201024))
#define UART0_FBRD      ((volatile unsigned int*)(MMIO_BASE+0x00201028))
#define UART0_LCRH      ((volatile unsigned int*)(MMIO_BASE+0x0020102C))
#define UART0_CR        ((volatile unsigned int*)(MMIO_BASE+0x00201030))
#define UART0_IMSC      ((volatile unsigned int*)(MMIO_BASE+0x00201038))
#define UART0_ICR       ((volatile unsigned int*)(MMIO_BASE+0x00201044))

#define VIDEOCORE_MBOX  (MMIO_BASE+0x0000B880)
#define MBOX_READ       ((volatile unsigned int*)(VIDEOCORE_MBOX+0x0))
#define MBOX_POLL       ((volatile unsigned int*)(VIDEOCORE_MBOX+0x10))
#define MBOX_SENDER     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x14))
#define MBOX_STATUS     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x18))
#define MBOX_CONFIG     ((volatile unsigned int*)(VIDEOCORE_MBOX+0x1C))
#define MBOX_WRITE      ((volatile unsigned int*)(VIDEOCORE_MBOX+0x20))
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000
#define MBOX_REQUEST    0
#define MBOX_CH_PROP    8
#define MBOX_TAG_SETCLKRATE     0x38002
#define MBOX_TAG_LAST           0

static unsigned long MMIO_BASE;
volatile unsigned int  __attribute__((aligned(16))) mbox[36];

/**
 * Set baud rate and characteristics (115200 8N1) and map to GPIO
 */
void dbg_uart_init()
{
    register unsigned int r;

    /* detect board */
    __asm__ __volatile__ ("mrs %0, midr_el1" : "=r" (r));
    switch(r&0xFFF0) {
        case 0xD030: MMIO_BASE = 0x3F000000; break;     /* Raspberry Pi 3 */
        default:     MMIO_BASE = 0xFE000000; break;     /* Raspberry Pi 4 */
    }

    /* initialize UART */
    *UART0_CR = 0;

    mbox[0] = 9*4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETCLKRATE;
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = 2;           /* UART clock */
    mbox[6] = 4000000;     /* 4Mhz */
    mbox[7] = 0;           /* clear turbo */
    mbox[8] = MBOX_TAG_LAST;
    r = (((unsigned int)((unsigned long)&mbox)&~0xF) | MBOX_CH_PROP);
    do{__asm__ __volatile__("nop");}while(*MBOX_STATUS & MBOX_FULL);
    *MBOX_WRITE = r;
    while(1) {
        do{__asm__ __volatile__("nop");}while(*MBOX_STATUS & MBOX_EMPTY);
        if(r == *MBOX_READ) break;
    }

    /* map UART0 to GPIO pins */
    r=*GPFSEL1;
    r&=~((7<<12)|(7<<15));
    r|=(4<<12)|(4<<15);
    *GPFSEL1 = r;
    *GPPUD = 0;
    r=150; while(r--) { __asm__ __volatile__("nop"); }
    *GPPUDCLK0 = (1<<14)|(1<<15);
    r=150; while(r--) { __asm__ __volatile__("nop"); }
    *GPPUDCLK0 = 0;

    *UART0_ICR = 0x7FF;    /* clear interrupts */
    *UART0_IBRD = 2;       /* 115200 baud */
    *UART0_FBRD = 0xB;
    *UART0_LCRH = 0b11<<5; /* 8n1 */
    *UART0_CR = 0x301;     /* enable Tx, Rx, FIFO */
}

/**
 * Send a character
 */
void dbg_uart_putc(unsigned int c) {
    do{__asm__ __volatile__("nop");}while(*UART0_FR&0x20);
    *UART0_DR=c;
}

/**
 * Receive a character
 */
char dbg_uart_getc() {
    char r;
    do{__asm__ __volatile__("nop");}while(*UART0_FR&0x10);
    r=(char)(*UART0_DR);
    return r=='\r'?'\n':r;
}

