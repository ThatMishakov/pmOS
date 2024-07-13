#include <stdarg.h>
#include <ucontext.h>

// defined in __ucontext_start.S
void __ucontext_start();

static void push_stack(unsigned long **stack, unsigned long value)
{
    *stack -= 1;
    **stack = value;
}

#define RISCV_PARAM_REGS 8

#define RISCV_REG_RA 0
#define RISCV_REG_SP 1
#define RISCV_REG_FP 7
#define RISCV_REG_S1 8
#define RISCV_REG_S2 17
#define RISCV_REG_S10 25


void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
    va_list ap;
    unsigned long *sp;

    printf("makecontext ss_sp %lx ss_size %lx ucp %lx\n", ucp->uc_stack.ss_sp, ucp->uc_stack.ss_size, ucp);
    sp = (unsigned long *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size / sizeof(unsigned long);
    if (argc > RISCV_PARAM_REGS && argc % 2 != 0) {
        // Align stack
        push_stack(&sp, 0);
    }

    push_stack(&sp, 0); // End of stack frame
    push_stack(&sp, 0); // No return address
    ucp->uc_mcontext.regs[RISCV_REG_FP] = (unsigned long)sp + sizeof(long)*2; // Frame pointer

    va_start(ap, argc);
    for (int i = 0; i < argc; i++) {
        if (i < RISCV_PARAM_REGS) {
            // Pass in saved registers, starting with s2
            ucp->uc_mcontext.regs[RISCV_REG_S2 + i] = va_arg(ap, unsigned long);
        } else {
            push_stack(&sp, va_arg(ap, unsigned long));
        }
    }

    va_end(ap);

    ucp->uc_mcontext.regs[RISCV_REG_RA] = (unsigned long)__ucontext_start; // Trampoline
    ucp->uc_mcontext.regs[RISCV_REG_SP] = (unsigned long)sp;               // Stack pointer
    ucp->uc_mcontext.regs[RISCV_REG_S1] = (unsigned long)ucp->uc_link;     // link to next context
    ucp->uc_mcontext.regs[RISCV_REG_S10] = (unsigned long)func;            // Function pointer

    // Set partial to true, since only saved registers are set
    ucp->uc_mcontext.partial = 1;
}
