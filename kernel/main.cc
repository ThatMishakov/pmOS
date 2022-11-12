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
#include <memory/paging.hh>

Kernel_Entry_Data* kdata;

extern "C" int main(Kernel_Entry_Data* d)
{
    nx_bit_enabled = d->flags & 0x01;
    t_print_bochs("nx_bit_enabled %h\n", nx_bit_enabled);

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
