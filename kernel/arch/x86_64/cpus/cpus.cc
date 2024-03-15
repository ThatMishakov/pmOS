#include "cpus.hh"
#include <sched/sched.hh>
#include <x86_asm.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/apic.hh>
#include <kernel/errors.h>
#include <interrupts/gdt.hh>
#include <processes/syscalls.hh>
#include "sse.hh"
#include <memory/palloc.hh>
#include <sched/timers.hh>
#include <kern_logger/kern_logger.hh>

void program_syscall()
{
    write_msr(0xC0000081, ((u64)(R0_CODE_SEGMENT) << 32) | ((u64)(R3_LEGACY_CODE_SEGMENT) << 48)); // STAR (segments for user and kernel code)
    write_msr(0xC0000082, (u64)&syscall_entry); // LSTAR (64 bit entry point)
    write_msr(0xC0000084, (u32)~0x0); // SFMASK (mask for %rflags)

    // Enable SYSCALL/SYSRET in EFER register
    u64 efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | (0x01 << 0));

    // This doesn't work on AMD CPUs
    // u64 cpuid = ::cpuid(1);
    // if (cpuid & (u64(1) << (11 + 32))) {
    //     write_msr(0x174, R0_CODE_SEGMENT); // IA32_SYSENTER_CS
    //     write_msr(0x175, (u64)get_cpu_struct()->kernel_stack_top); // IA32_SYSENTER_ESP
    //     write_msr(0x176, (u64)&sysenter_entry); // IA32_SYSENTER_EIP
    // }
}

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
    c->cpu_gdt.tss_descriptor.tss()->ist2 = (u64)c->debug_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist1 = (u64)c->kernel_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->rsp0 = (u64)c->kernel_stack.get_stack_top();
    
    loadTSS(TSS_OFFSET);

    c->lapic_id = get_lapic_id();

    // This creates a *fat* use-after-free race condition (which hopefully shouldn't lead to a lot of crashes because
    // current malloc is not that good) during the initialization but I don't like the idea of putting a lock here
    // TODO: Think of something

    cpus.push_back(c);
    c->cpu_id = cpus.size();

    serial_logger.printf("Initializing idle task\n");
    
    init_idle();
    c->current_task = c->idle_task;

    program_syscall();
    set_idt();
    enable_apic();
    enable_sse();
}

extern "C" void cpu_start_routine()
{
    init_per_cpu();

    set_idt();
    enable_apic();

    get_cpu_struct()->lapic_id = get_lapic_id();

    enable_sse();

    // IMO this is ugly
    const auto& idle = get_cpu_struct()->idle_task;
    get_cpu_struct()->current_task = idle;
    const auto idle_pt = klib::dynamic_pointer_cast<x86_Page_Table>(idle->page_table);
    idle_pt->atomic_active_sum(1);
    setCR3(idle_pt->get_cr3());
    get_cpu_struct()->current_task->switch_to();
    reschedule();
    
    global_logger.printf("[Kernel] Initialized CPU %h\n", get_lapic_id());
}

extern "C" u64* get_kern_stack_top()
{
    return get_cpu_struct()->kernel_stack.get_stack_top();
}

void init_scheduling()
{
    serial_logger.printf("Initializing APIC\n");
    prepare_apic();

    serial_logger.printf("Initializing interrupts\n");
    init_interrupts();

    serial_logger.printf("Initializing per-CPU structures\n");
    init_per_cpu();

    serial_logger.printf("Scheduling initialized\n");
}