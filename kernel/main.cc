#include <kernel/com.h>
#include "gdt.hh"
#include "utils.hh"
#include "misc.hh"
#include "mem.hh"
#include "interrupts.hh"
#include "malloc.hh"
#include "sched.hh"
#include "messaging.hh"
#include "free_page_alloc.hh"
#include <kernel/errors.h>
#include "vga.hh"
#include "messaging.hh"

Kernel_Entry_Data* kdata;

extern "C" int main(Kernel_Entry_Data* d)
{
    kdata = d;
    init_gdt();
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();
    free_page_alloc_init();
    init_kernel_ports();
    init_video();
    init_scheduling();
    init_interrupts();
    return 0;
}
