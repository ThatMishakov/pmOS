#include <x86_asm.hh>
#include <kern_logger/kern_logger.hh>

extern "C" void allow_access_user()
{
    // On x86, access to user space is always allowed
}

extern "C" void disallow_access_user()
{
}

void halt()
{
    asm("hlt");
}

void printc(int c)
{
    bochs_printc(c);
}

extern "C" void dbg_uart_putc(char c)
{
    printc(c);
}

struct stack_frame {
    stack_frame* next;
    void* return_addr;
};

void print_stack_trace(Logger& logger, stack_frame * s)
{
    logger.printf("Stack trace:\n");
    for (; s != NULL; s = s->next)
        logger.printf("  -> %h\n", (u64)s->return_addr);
}

void print_stack_trace(Logger& logger)
{
    struct stack_frame* s;
    __asm__ volatile("movq %%rbp, %0": "=a" (s));
    print_stack_trace(logger, s);
}

extern "C" void print_stack_trace()
{
    struct stack_frame* s;
    __asm__ volatile("movq %%rbp, %0": "=a" (s));
    print_stack_trace(bochs_logger, s);
}