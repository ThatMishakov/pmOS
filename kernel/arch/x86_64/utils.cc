/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kern_logger/kern_logger.hh>
#include <x86_asm.hh>

extern "C" void allow_access_user()
{
    // On x86, access to user space is always allowed
}

extern "C" void disallow_access_user() {}

void halt() { asm("hlt"); }

bool serial_initiated  = false;
bool serial_functional = true;
Spinlock serial_lock;

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define PORT 0x3f8 // COM1

int is_transmit_empty() { return inb(PORT + 5) & 0x20; }

void dbg_uart_init()
{
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
        ::
            : "rax", "rdx");
}

static int init_serial()
{
    serial_initiated = true;

    dbg_uart_init();
    return 0;
}

extern "C" void dbg_uart_putc(unsigned int c)
{
    if (!serial_initiated)
        init_serial();

    __asm__ __volatile__("movl $10000,%%ecx;movw $0x3fd,%%dx;"
                         "1:inb %%dx, %%al;pause;"
                         "cmpb $0xff,%%al;je 2f;"
                         "dec %%ecx;jz 2f;"
                         "andb $0x20,%%al;jz 1b;"
                         "subb $5,%%dl;movb %%bl, %%al;outb %%al, %%dx;2:" ::"b"(c)
                         : "rax", "rcx", "rdx");
}

void printc(int c) { dbg_uart_putc(c); }

struct stack_frame {
    stack_frame *next;
    void *return_addr;
};

void print_stack_trace(Logger &logger, stack_frame *s)
{
    logger.printf("Stack trace:\n");
    for (; s != NULL; s = s->next)
        logger.printf("  -> %h\n", (u64)s->return_addr);
}

void print_stack_trace(Logger &logger)
{
    struct stack_frame *s;
    __asm__ volatile("movq %%rbp, %0" : "=a"(s));
    print_stack_trace(logger, s);
}

extern "C" void print_stack_trace()
{
    struct stack_frame *s;
    __asm__ volatile("movq %%rbp, %0" : "=a"(s));
    print_stack_trace(bochs_logger, s);
}