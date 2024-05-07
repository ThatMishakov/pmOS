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

void printc(int c) { bochs_printc(c); }

extern "C" void dbg_uart_putc(char c) { printc(c); }

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