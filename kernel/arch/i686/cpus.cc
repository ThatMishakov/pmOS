#include <cpus/sse.hh>
#include <interrupts/apic.hh>
#include <interrupts/gdt.hh>
#include <interrupts/interrupts.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/vmm.hh>
#include <paging/x86_temp_mapper.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <x86_asm.hh>

using namespace kernel;

extern "C" void double_fault_isr();

void program_syscall()
{
    // TODO I guess..?
}

extern u32 idle_cr3;

bool setup_stacks(CPU_Info *c)
{
    if (!c->kernel_stack)
        return false;

    c->kernel_stack_top = c->kernel_stack.get_stack_top();
    c->tss.esp0         = (u32)c->kernel_stack_top;

    c->cpu_gdt.tss = tss_to_base(&c->tss);

    if (!c->double_fault_stack)
        return false;

    c->double_fault_tss.esp = (u32)c->double_fault_stack.get_stack_top();
    c->double_fault_tss.eip = (u32)double_fault_isr;
    c->double_fault_tss.cr3 = idle_cr3;

    c->cpu_gdt.double_fault_tss = tss_to_base(&c->double_fault_tss);

    return true;
}

extern bool cpu_struct_works;

void init_per_cpu(u64 lapic_id)
{
    detect_sse();

    CPU_Info *c = new CPU_Info();
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);

    cpu_struct_works = true;

    if (!setup_stacks(c))
        panic("Failed to setup stacks\n");

    loadTSS();

    c->lapic_id = lapic_id;

    if (!cpus.push_back(c))
        panic("Failed to reserve memory for cpus vector in init_per_cpu()\n");

    c->cpu_id = cpus.size() - 1;

    serial_logger.printf("Initializing idle task\n");

    auto r = init_idle(c);
    if (r)
        panic("Failed to initialize idle task: %i\n", r);
    c->current_task = c->idle_task;
    c->idle_task->page_table->apply_cpu(c);

    program_syscall();
    set_idt();
    enable_apic();
    enable_sse();

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");

    c->temp_mapper = create_temp_mapper(temp_mapper_start, getCR3());
    if (!c->temp_mapper)
        panic("Failed to create temp mapper\n");
}

void init_scheduling_on_bsp()
{
    serial_logger.printf("Initializing APIC\n");
    prepare_apic();

    serial_logger.printf("Initializing interrupts\n");
    init_interrupts();

    serial_logger.printf("Initializing per-CPU structures\n");
    init_per_cpu(get_lapic_id());

    serial_logger.printf("Scheduling initialized\n");
}