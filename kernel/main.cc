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
    init_gdt();
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();
    free_page_alloc_init();
    
    t_print("Initiing scheduling!\n");
    init_scheduling();
    t_print("Initiing interrupts!\n");
    init_interrupts();
    return 0;
}
