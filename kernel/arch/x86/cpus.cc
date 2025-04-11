#include <cpus/ipi.hh>
#include <interrupts/apic.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <x86_utils.hh>

using namespace kernel::sched;
using namespace kernel::paging;
using namespace kernel::x86::interrupts::lapic;
using namespace kernel::log;

extern int kernel_pt_generation;
extern int kernel_pt_active_cpus_count[2];

void park_self()
{
    auto c    = get_cpu_struct();
    auto task = c->current_task;

    task->page_table->unapply_cpu(get_cpu_struct());
    task->before_task_switch();

    // Kill TLB shootdowns
    int kernel_pt_gen = c->kernel_pt_generation;
    __atomic_sub_fetch(&kernel_pt_active_cpus_count[kernel_pt_gen], 1, __ATOMIC_RELEASE);
    c->kernel_pt_generation = -1;

    serial_logger.printf("Parking cpu %x\n", c->lapic_id);

    __atomic_store_n(&c->online, false, __ATOMIC_RELEASE);
    __asm__ volatile("wbinvd");
    __asm__ volatile("hlt");
}

void deactivate_page_table()
{
    auto c = get_cpu_struct();

    int kernel_pt_gen = c->kernel_pt_generation;
    __atomic_sub_fetch(&kernel_pt_active_cpus_count[kernel_pt_gen], 1, __ATOMIC_RELEASE);
    c->kernel_pt_generation = -1;
}

extern "C" CPU_Info *find_cpu_info()
{
    auto lapic_id = get_lapic_id();
    for (auto c: cpus)
        if (c->lapic_id == lapic_id)
            return c;

    assert(false);
    __builtin_unreachable();
}

extern kernel::pmm::phys_page_t acpi_trampoline_page;

void smp_wake_everyone_else_up()
{
    uint32_t vector = acpi_trampoline_page >> 12;

    apic_write_reg(APIC_ICR_HIGH, 0);

    // Send to *vector* vector with Assert level and All Excluding Self
    // shorthand
    apic_write_reg(APIC_ICR_LOW, vector | (0x01 << 14) | (0b11 << 18) | (0b101) << 8);
    apic_write_reg(APIC_ICR_LOW, vector | (0x01 << 14) | (0b11 << 18) | (0b110) << 8);
}

namespace kernel::sched
{
void check_synchronous_ipis();
}

void stop_cpus()
{
    serial_logger.printf("Stopping cpus\n");
    auto my_cpu = get_cpu_struct();
    for (auto cpu: cpus) {
        if (cpu == my_cpu)
            continue;

        __atomic_or_fetch(&cpu->ipi_mask, CPU_Info::IPI_CPU_PARK, __ATOMIC_ACQUIRE);
        send_ipi_fixed(ipi_invalidate_tlb_int_vec, cpu->lapic_id);
    }

    for (auto cpu: cpus) {
        if (cpu == my_cpu)
            continue;

        while (__atomic_load_n(&cpu->online, __ATOMIC_RELAXED)) {
            x86_pause();
            check_synchronous_ipis();
        }
    }
}