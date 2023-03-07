#include "cpus.hh"
#include <sched/sched.hh>
#include <asm.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/apic.hh>
#include <kernel/errors.h>
#include <interrupts/gdt.hh>
#include <processes/syscalls.hh>
#include "sse.hh"
#include <memory/palloc.hh>

klib::vector<CPU_Desc> cpus;

void init_per_cpu()
{
    CPU_Info* c = new CPU_Info;
    loadGDT(&c->cpu_gdt);

    write_msr(0xC0000101, (u64)c);

    TSS* tss = new TSS();
    c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64) tss, sizeof(TSS), 0x89, 0x02);

    c->kernel_stack_top = c->kernel_stack.get_stack_top();

    c->cpu_gdt.tss_descriptor.tss()->ist7 = (u64)c->double_fault_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist6 = (u64)c->nmi_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist5 = (u64)c->machine_check_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist1 = (u64)c->kernel_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->rsp0 = (u64)c->kernel_stack.get_stack_top();
    
    loadTSS(TSS_OFFSET);

    cpus.push_back({c, get_lapic_id()});

    init_idle();

    program_syscall();
}

extern "C" void cpu_start_routine()
{
    init_per_cpu();
    set_idt();
    enable_apic();

    get_cpu_struct()->lapic_id = get_lapic_id();

    enable_sse();

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