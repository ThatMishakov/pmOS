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