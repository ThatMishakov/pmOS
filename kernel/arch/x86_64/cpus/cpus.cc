/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpus.hh"

#include "sse.hh"

#include <interrupts/apic.hh>
#include <interrupts/gdt.hh>
#include <interrupts/interrupts.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/palloc.hh>
#include <memory/vmm.hh>
#include <processes/syscalls.hh>
#include <sched/sched.hh>
#include <sched/timers.hh>
#include <x86_asm.hh>
#include <stdlib.h>

using namespace kernel;

void program_syscall()
{
    write_msr(0xC0000081, ((u64)(R0_CODE_SEGMENT) << 32) | ((u64)(R3_LEGACY_CODE_SEGMENT) << 48));
    // STAR (segments for user and kernel code)
    write_msr(0xC0000082, (u64)&syscall_entry); // LSTAR (64 bit entry point)
    write_msr(0xC0000084, (u32)~0x0);           // SFMASK (mask for %rflags)

    // Enable SYSCALL/SYSRET in EFER register
    u64 efer = read_msr(0xC0000080);
    write_msr(0xC0000080, efer | (0x01 << 0));

    // This doesn't work on AMD CPUs
    // u64 cpuid = ::cpuid(1);
    // if (cpuid & (u64(1) << (11 + 32))) {
    //     write_msr(0x174, R0_CODE_SEGMENT); // IA32_SYSENTER_CS
    //     write_msr(0x175, (u64)get_cpu_struct()->kernel_stack_top); //
    //     IA32_SYSENTER_ESP write_msr(0x176, (u64)&sysenter_entry); //
    //     IA32_SYSENTER_EIP
    // }
}

extern "C" void _cpu_entry(u64 limine_struct);
void *get_cpu_start_func() { return (void *)_cpu_entry; }

void init_per_cpu(uint32_t lapic_id)
{
    CPU_Info *c = new CPU_Info;
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    loadGDT(&c->cpu_gdt);
    write_msr(0xC0000101, (u64)c);

    TSS *tss                  = new TSS();
    if (!tss)
        panic("Couldn't allocate memory for TSS\n");
    
    c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64)tss, sizeof(TSS), 0x89, 0x02);

    c->kernel_stack_top = c->kernel_stack.get_stack_top();

    c->cpu_gdt.tss_descriptor.tss()->ist7 = (u64)c->double_fault_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist6 = (u64)c->nmi_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist5 = (u64)c->machine_check_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist2 = (u64)c->debug_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist1 = (u64)c->kernel_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->rsp0 = (u64)c->kernel_stack.get_stack_top();

    loadTSS(TSS_OFFSET);

    c->lapic_id = lapic_id;
    assert(lapic_id == (get_lapic_id() >> 24));

    // This creates a *fat* use-after-free race condition (which hopefully
    // shouldn't lead to a lot of crashes because current malloc is not that
    // good) during the initialization but I don't like the idea of putting a
    // lock here
    // TODO: Think of something

    if (!cpus.push_back(c))
        panic("Failed to reserve memory for cpus vector in init_per_cpu()\n");
    
    c->cpu_id = cpus.size() - 1;

    serial_logger.printf("Initializing idle task\n");

    auto r = init_idle(c);
    if (r)
        panic("Failed to initialize idle task: %i\n", r);
    c->current_task = c->idle_task;

    program_syscall();
    set_idt();
    enable_apic();
    enable_sse();

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");
    
    c->temp_mapper          = x86_PAE_Temp_Mapper(temp_mapper_start, getCR3());
}

u64 bootstrap_cr3 = 0;
klib::vector<u64> initialize_cpus(const klib::vector<u64> &lapic_ids)
{
    bootstrap_cr3 = getCR3();

    if (!cpus.reserve(lapic_ids.size())){
        serial_logger.printf("Failed to reserve memory for cpus vector in initialize_cpus()\n");
        abort();
    }


    klib::vector<u64> ret;
    if (!ret.reserve(lapic_ids.size())) {
        serial_logger.printf("Failed to reserve memory for ret vector in initialize_cpus()\n");
        abort();
    }

    if (!cpus.reserve(cpus.size() + lapic_ids.size())) {
        serial_logger.printf("Failed to reserve memory for cpus vector in initialize_cpus()\n");
        abort();
    }
    
    for (const auto &id: lapic_ids) {
        CPU_Info *c               = new CPU_Info;
        TSS *tss                  = new TSS();
        c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64)tss, sizeof(TSS), 0x89, 0x02);

        c->kernel_stack_top = c->kernel_stack.get_stack_top();

        c->cpu_gdt.tss_descriptor.tss()->ist7 = (u64)c->double_fault_stack.get_stack_top();
        c->cpu_gdt.tss_descriptor.tss()->ist6 = (u64)c->nmi_stack.get_stack_top();
        c->cpu_gdt.tss_descriptor.tss()->ist5 = (u64)c->machine_check_stack.get_stack_top();
        c->cpu_gdt.tss_descriptor.tss()->ist2 = (u64)c->debug_stack.get_stack_top();
        c->cpu_gdt.tss_descriptor.tss()->ist1 = (u64)c->kernel_stack.get_stack_top();
        c->cpu_gdt.tss_descriptor.tss()->rsp0 = (u64)c->kernel_stack.get_stack_top();

        c->lapic_id = id;
        cpus.push_back(c);
        c->cpu_id = cpus.size() - 1;

        auto t = init_idle(c);
        if (t)
            panic("Failed to initialize idle task: %i\n", t);
        
        c->current_task = c->idle_task;

        void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
        c->temp_mapper          = x86_PAE_Temp_Mapper(temp_mapper_start, getCR3());

        c->kernel_stack_top[-1] = (u64)c;
        ret.push_back((u64)c->kernel_stack_top);
    }

    return ret;
}

extern size_t booted_cpus;
extern bool boot_barrier_start;

Spinlock l;
extern "C" void cpu_start_routine(CPU_Info *c)
{
    loadGDT(&c->cpu_gdt);
    write_msr(0xC0000101, (u64)c);
    loadTSS(TSS_OFFSET);
    program_syscall();
    set_idt();
    enable_apic();
    enable_sse();

    const auto &idle               = get_cpu_struct()->idle_task;
    get_cpu_struct()->current_task = idle;
    const auto idle_pt             = klib::dynamic_pointer_cast<x86_Page_Table>(idle->page_table);
    idle_pt->atomic_active_sum(1);
    get_cpu_struct()->current_task->switch_to();
    reschedule();

    u32 lapic_id = get_lapic_id() >> 24;
    assert(c->lapic_id == lapic_id);
    global_logger.printf("[Kernel] Initialized CPU %h\n", lapic_id);

    __atomic_add_fetch(&booted_cpus, 1, __ATOMIC_SEQ_CST);

    // Wait for bootstrap hart to do the final initialization
    while (!__atomic_load_n(&boot_barrier_start, __ATOMIC_SEQ_CST))
        ;

    serial_logger.printf("CPU %h entering userspace/idle\n", lapic_id);
}

extern "C" u64 *get_kern_stack_top() { return get_cpu_struct()->kernel_stack.get_stack_top(); }

void init_scheduling(u64 bootstap_apic_id)
{
    serial_logger.printf("Initializing APIC\n");
    prepare_apic();

    serial_logger.printf("Initializing interrupts\n");
    init_interrupts();

    serial_logger.printf("Initializing per-CPU structures\n");
    init_per_cpu(bootstap_apic_id);

    serial_logger.printf("Scheduling initialized\n");
}