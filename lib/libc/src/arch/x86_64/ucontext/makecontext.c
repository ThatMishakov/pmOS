/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ucontext.h>
#include <stdarg.h>

// defined in ucontext_start.S
void __ucontext_start();

static void push_stack(unsigned long **stack, unsigned long value) {
    *stack -= 1;
    **stack = value;
}

void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...) {
    va_list ap;
    unsigned long *sp;

    sp = (unsigned long *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size / sizeof(unsigned long);
    push_stack(&sp, 0); // End of stack frame
    if (argc > 6 && argc % 2 == 0) {
        // Align stack
        push_stack(&sp, 0);
    }
    
    va_start(ap, argc);
    for (int i = 0; i < argc; i++) {
        switch (i) {
            case 0:
                ucp->uc_mcontext.rbx = va_arg(ap, unsigned long);
                break;
            case 1:
                ucp->uc_mcontext.r12 = va_arg(ap, unsigned long);
                break;
            case 2:
                ucp->uc_mcontext.r13 = va_arg(ap, unsigned long);
                break;
            case 3:
                ucp->uc_mcontext.r14 = va_arg(ap, unsigned long);
                break;
            case 4:
                ucp->uc_mcontext.r15 = va_arg(ap, unsigned long);
                break;
            case 5:
                ucp->uc_mcontext.rbp = va_arg(ap, unsigned long);
                break;
            default:
                push_stack(&sp, va_arg(ap, unsigned long));
                break;
        }
    }

    va_end(ap);

    push_stack(&sp, (unsigned long)func); // Function pointer
    push_stack(&sp, (unsigned long)ucp->uc_link); // link to next context
    push_stack(&sp, (unsigned long)__ucontext_start); // Trampoline return address
    ucp->uc_mcontext.rsp = (unsigned long)sp;
}