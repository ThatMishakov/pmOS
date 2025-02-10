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
bool serial_functional = false;
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

static int init_serial()
{
    serial_initiated = true;

    outb(PORT + 1, 0x00); // Disable all interrupts
    outb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x0C); // Set divisor to 12 (lo byte) 9600 baud
    outb(PORT + 1, 0x00); //                  (hi byte)
    outb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
    // outb(PORT + 4, 0x1E); // Set in loopback mode, test the serial chip
    // outb(PORT + 0, 0xAE); // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // // Check if serial is faulty (i.e: not same byte as sent)
    // if (inb(PORT + 0) != 0xAE) {
    //     return 1;
    // }

    serial_functional = true;

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);
    return 0;
}

extern "C" void dbg_uart_putc(unsigned int c)
{
    if (!serial_initiated) {
        Auto_Lock_Scope guard(serial_lock);
        if (!serial_initiated) {
            if (init_serial() != 0)
                return;
        }
    }

    if (!serial_functional)
        return;

    while (is_transmit_empty() == 0)
        ;

    outb(PORT, c);
}

// extern "C" void dbg_uart_putc(unsigned int c)
// {
//     bochs_printc(c);
// }

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
    #ifdef __x86_64__
    __asm__ volatile("movq %%rbp, %0" : "=a"(s));
    #else
    __asm__ volatile("movl %%ebp, %0" : "=a"(s));
    #endif
    print_stack_trace(logger, s);
}

extern "C" void print_stack_trace()
{
    struct stack_frame *s;
    #ifdef __x86_64__
    __asm__ volatile("movq %%rbp, %0" : "=a"(s));
    #else
    __asm__ volatile("movl %%ebp, %0" : "=a"(s));
    #endif
    print_stack_trace(bochs_logger, s);
}