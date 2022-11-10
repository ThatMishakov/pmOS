#include <kernel/com.h>
#include <interrupts/gdt.hh>
#include "utils.hh"
#include "misc.hh"
#include <memory/mem.hh>
#include <interrupts/interrupts.hh>
#include <memory/malloc.hh>
#include <processes/sched.hh>
#include <messaging/messaging.hh>
#include <kernel/errors.h>
#include <objects/crti.h>

Kernel_Entry_Data* kdata;

extern "C" int main(Kernel_Entry_Data* d)
{
    kdata = d;
    init_gdt();
    palloc.init(d->mem_bitmap, d->mem_bitmap_size);
    palloc.init_after_paging();
    _init();

    init_kernel_ports();
    init_scheduling();
    init_interrupts();
    return 0;
}
