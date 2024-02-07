#include <pmos/debug.h>
#include <stdio.h>

typedef struct stack_frame {
    struct stack_frame* next;
    void* return_addr;
} stack_frame;

void pmos_print_stack_trace()
{
    // fprintf(stderr, "Stack trace:\n");
    // struct stack_frame* s;
    // __asm__ volatile("movq %%rbp, %0": "=a" (s));
    // for (; s != NULL; s = s->next)
    //     fprintf(stderr, "  -> %lx\n", (uint64_t)s->return_addr);
}