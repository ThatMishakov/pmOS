#include "cpus.hh"
#include <sched/sched.hh>
#include <asm.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/apic.hh>
#include <kernel/errors.h>
#include <interrupts/gdt.hh>

klib::vector<CPU_Desc> cpus;

void init_per_cpu()
{
    CPU_Info* c = new CPU_Info;
    write_msr(0xC0000101, (u64)c);
    cpus.push_back({c, get_lapic_id()});

    init_idle();
}

extern "C" void cpu_start_routine()
{
    init_gdt();
    init_per_cpu();
    set_idt();
    init_kernel_stack();
    enable_apic();

    find_new_process();
}

ReturnStr<u64> cpu_configure(u64 type, UNUSED u64 arg)
{
    ReturnStr<u64> result = {ERROR_GENERAL, 0};
    switch (type) {
    case 0:
        result.result = SUCCESS;
        result.val = (u64)&cpu_startup_entry;
        break;
    default:
        result.result = ERROR_NOT_SUPPORTED;
    };
    return result;
}