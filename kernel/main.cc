#include <kernel/com.h>
#include <interrupts/gdt.hh>
#include "utils.hh"
#include "misc.hh"
#include <memory/mem.hh>
#include <interrupts/interrupts.hh>
#include <memory/malloc.hh>
#include <sched/sched.hh>
#include <messaging/messaging.hh>
#include <kernel/errors.h>
#include <objects/crti.h>
#include <memory/paging.hh>
#include <interrupts/apic.hh>

Kernel_Entry_Data* kdata;

extern "C" int main(Kernel_Entry_Data* d)
{
    nx_bit_enabled = d->flags & 0x01;
    kdata = d;
    kernel_pframe_allocator.init(d->mem_bitmap, d->mem_bitmap_size);
    kernel_pframe_allocator.init_after_paging();
    _init();
    init_kernel_ports();
    prepare_apic();
    init_scheduling();
    init_interrupts();

    get_cpu_struct()->lapic_id = get_lapic_id();
    enable_sse();

    start_scheduler();
    
    return 0;
}
