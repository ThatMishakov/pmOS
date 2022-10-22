#include "common/com.h"
#include "gdt.hh"
#include "utils.hh"
#include "misc.hh"
#include "mem.hh"
#include "interrupts.hh"
#include "malloc.hh"
#include "sched.hh"
#include "messaging.hh"
#include "free_page_alloc.hh"
#include "common/errors.h"

Kernel_Entry_Data* kdata;

extern "C" int main(Kernel_Entry_Data* d)
{
    kdata = d;
    t_print("Loading GDT\n");
    asm("xchgw %bx, %bx");
    init_gdt();
    t_print("Loading memory\n");
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();
    free_page_alloc_init();
    t_print("Loading scheduling\n");
    init_scheduling();
    init_interrupts();
    return 0;
}
