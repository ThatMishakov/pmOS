#include <acpi/acpi.h>
#include <cpus/sse.hh>
#include <interrupts/apic.hh>
#include <interrupts/gdt.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/ioapic.hh>
#include <interrupts/pic.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/vmm.hh>
#include <paging/x86_temp_mapper.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <x86_asm.hh>

using namespace kernel;
using namespace kernel::x86;
using namespace kernel::sched;
using namespace kernel::log;
using namespace kernel::ia32::interrupts;
using namespace kernel::proc;
using namespace kernel::ia32::paging;
using namespace kernel::x86::interrupts;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::paging;

extern "C" void double_fault_isr();

void program_syscall()
{
    // TODO I guess..?
}

static bool setup_stacks(CPU_Info *c)
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

void smp_wake_everyone_else_up();

namespace kernel::sched
{
extern bool cpu_struct_works;
}

void init_per_cpu(u64 lapic_id)
{
    sse::detect_sse();

    CPU_Info *c = new CPU_Info();
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);

    sched::cpu_struct_works = true;

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
    sse::enable_sse();

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");

    c->temp_mapper = create_temp_mapper(temp_mapper_start, getCR3());
    if (!c->temp_mapper)
        panic("Failed to create temp mapper\n");
}

extern int kernel_pt_active_cpus_count[2];

extern "C" void smp_main(CPU_Info *c)
{
    serial_logger.printf("Entered CPU %i\n", c->cpu_id);

    gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);
    unbusyTSS(&c->cpu_gdt);
    loadTSS();
    program_syscall();
    set_idt();
    enable_apic();
    sse::enable_sse();

    if (c->to_restore_on_wakeup) {
        c->current_task->regs = *c->to_restore_on_wakeup;
        c->to_restore_on_wakeup.reset();
    }
    c->current_task->page_table->apply_cpu(c);
    c->current_task->page_table->apply();
    c->current_task->after_task_switch();

    assert(c->kernel_pt_generation == -1);
    c->kernel_pt_generation = __atomic_load_n(&kernel_pt_generation, __ATOMIC_ACQUIRE);
    __atomic_add_fetch(&kernel_pt_active_cpus_count[c->kernel_pt_generation], 1, __ATOMIC_RELAXED);
    c->online = true;

    if (__atomic_load_n(&c->ipi_mask, __ATOMIC_ACQUIRE))
        c->ipi_reschedule();
}

extern bool serial_initiated;
extern bool serial_functional;

extern "C" void acpi_main()
{
    serial_initiated = false;
    serial_logger.printf("Printing from C++ after waking up! (LAPIC ID %x)\n", get_lapic_id());

    auto c = cpus[0];
    init_PIC();

    gdt_set_cpulocal(c);
    loadGDT(&c->cpu_gdt);
    unbusyTSS(&c->cpu_gdt);
    loadTSS();
    program_syscall();
    set_idt();
    enable_apic();
    sse::enable_sse();

    if (c->to_restore_on_wakeup) {
        c->current_task->regs = *c->to_restore_on_wakeup;
        c->to_restore_on_wakeup.reset();
    }
    c->current_task->page_table->apply_cpu(c);
    c->current_task->page_table->apply();
    c->current_task->after_task_switch();

    assert(c->kernel_pt_generation == -1);
    c->kernel_pt_generation = __atomic_load_n(&kernel_pt_generation, __ATOMIC_ACQUIRE);
    __atomic_add_fetch(&kernel_pt_active_cpus_count[c->kernel_pt_generation], 1, __ATOMIC_RELAXED);
    c->online = true;

    if (__atomic_load_n(&c->ipi_mask, __ATOMIC_ACQUIRE))
        c->ipi_reschedule();

    smp_wake_everyone_else_up();
}

CPU_Info *prepare_cpu(unsigned idx, u32 lapic_id)
{
    serial_logger.printf("Found CPU LAPIC %x\n", lapic_id);

    CPU_Info *c = new CPU_Info();
    if (!c)
        panic("Couldn't allocate memory for CPU_Info\n");

    if (!setup_stacks(c))
        panic("Failed to set up stack for AP");

    c->lapic_id             = lapic_id;
    c->cpu_id               = idx;
    c->kernel_pt_generation = -1;

    auto r = init_idle(c);
    if (r)
        panic("Failed to initialize idle task: %i\n", r);
    c->current_task = c->idle_task;

    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(16, 4);
    if (!temp_mapper_start)
        panic("Failed to allocate memory for temp_mapper_start\n");

    c->temp_mapper = create_temp_mapper(temp_mapper_start, getCR3());
    if (!c->temp_mapper)
        panic("Failed to create temp mapper\n");

    c->online = false;

    return c;
}

MADT *get_madt();
void init_acpi_trampoline();

void init_smp()
{
    init_acpi_trampoline();

    uint32_t my_lapic_id = get_lapic_id();
    MADT *m              = get_madt();
    if (!m)
        panic("No MADT");

    unsigned idx = 1;

    serial_logger.printf("Initializing SMP\n");

    u32 offset = sizeof(MADT);
    u32 length = m->header.length;
    while (offset < length) {
        MADT_entry *e = (MADT_entry *)((char *)m + offset);
        offset += e->length;

        if (e->type == MADT_LAPIC_entry_type) {
            MADT_LAPIC_entry *ee = (MADT_LAPIC_entry *)e;
            uint32_t e_id        = ee->apic_id << 24;
            if (e_id == my_lapic_id)
                continue;

            auto c = prepare_cpu(idx++, e_id);
            if (!cpus.push_back(c))
                panic("Failed to add CPU\n");
        } else if (e->type == MADT_X2APIC_entry_type) {
            MADT_X2APIC_entry *ee = (MADT_X2APIC_entry *)e;
            uint32_t e_id         = ee->x2apic_id;

            if (e_id == my_lapic_id)
                continue;

            auto c = prepare_cpu(idx++, e_id);
            if (!cpus.push_back(c))
                panic("Failed to add CPU\n");
        }
    }

    smp_wake_everyone_else_up();
}

void init_scheduling_on_bsp()
{
    serial_logger.printf("Initializing APIC\n");
    prepare_apic();

    serial_logger.printf("Initializing interrupts\n");
    init_interrupts();

    serial_logger.printf("Initializing per-CPU structures\n");
    init_per_cpu(get_lapic_id());

    serial_logger.printf("Initializing I/O APICs\n");
    IOAPIC::init_ioapics();

    serial_logger.printf("Scheduling initialized\n");
}

extern u8 init_vec_begin;
extern u8 acpi_trampoline;
extern u8 init_vec_end;

// Relocations
extern u32 init_vec_jump_pmode;
extern u32 smp_trampoline_gdtr_addr;
extern u32 acpi_vec_jump_pmode;

// Variables
extern u32 smp_trampoline_cr3;
extern u32 smp_trampoline_trampoilne_flags;

constexpr u32 SMP_TRAMPOLINE_ENABLE_PAE = 0b001;
constexpr u32 SMP_TRAMPOLINE_ENABLE_NX  = 0b010;

pmm::phys_page_t acpi_trampoline_page = 0;

ReturnStr<u32> acpi_wakeup_vec()
{

    return Success((u32)acpi_trampoline_page + (&acpi_trampoline - &init_vec_begin));
}

void init_acpi_trampoline()
{
    auto page = pmm::alloc_pages(1, 0, kernel::pmm::AllocPolicy::ISA);
    if (!page) {
        panic("Kernel error: could not allocate memory for ACPI trampoline");
    }

    acpi_trampoline_page = page->get_phys_addr();

    init_vec_jump_pmode += acpi_trampoline_page;
    smp_trampoline_gdtr_addr += acpi_trampoline_page;
    acpi_vec_jump_pmode += acpi_trampoline_page;

    auto [result, new_cr3] = create_empty_cr3();
    if (result)
        panic("Failed to allocate cr3 for SMP trampoline");

    smp_trampoline_cr3       = new_cr3;
    Page_Table_Arguments pta = {
        .readable           = true,
        .writeable          = true,
        .user_access        = false,
        .execution_disabled = false,
    };

    if (use_pae) {
        smp_trampoline_trampoilne_flags |= SMP_TRAMPOLINE_ENABLE_PAE;
    }
    if (support_nx)
        smp_trampoline_trampoilne_flags |= SMP_TRAMPOLINE_ENABLE_NX;

    result = map_page(new_cr3, acpi_trampoline_page, (void *)acpi_trampoline_page, pta);
    if (result)
        panic("Failed to ID map init page");

    Temp_Mapper_Obj<char> t(request_temp_mapper());
    char *ptr = t.map(acpi_trampoline_page);
    memcpy(ptr, &init_vec_begin, (char *)&init_vec_end - (char *)&init_vec_begin);

    serial_logger.printf("Initialized SMP/ACPI trampoline vector at %x (%x)\n",
                         (u32)acpi_trampoline_page, acpi_wakeup_vec().val);
}