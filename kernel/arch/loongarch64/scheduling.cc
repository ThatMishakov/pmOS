#include <csr.hh>
#include <interrupts.hh>
#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <pmos/utility/scope_guard.hh>
#include <paging/loongarch64_paging.hh>
#include "iocsr.hh"

using namespace kernel;
using namespace kernel::sched;
using namespace kernel::log;
using namespace kernel::loongarch;

extern "C" void isr();

namespace kernel::sched
{
extern bool cpu_struct_works;
}

register sched::CPU_Info *cpu_struct __asm__("$tp");
static void set_cpu_struct(sched::CPU_Info *i) { cpu_struct = i; }

sched::CPU_Info *sched::get_cpu_struct() { return cpu_struct; }

void set_save0(sched::CPU_Info *i) { csrwr<loongarch::csr::SAVE0>(i); }

void ipi_enable();

void program_interrupts()
{
    csrwr<loongarch::csr::ECFG>(0x1fff);
    csrwr<loongarch::csr::EENTRY>(isr);
    ipi_enable();
}

void detect_supported_extensions();
void init_interrupts();

bool calculate_timer_frequency();

void init_scheduling(u64 cpu_id)
{
    sched::CPU_Info *i = new sched::CPU_Info();
    if (!i)
        panic("Could not allocate sched::CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->cpu_physical_id  = cpu_id;

    set_cpu_struct(i);
    set_save0(i);

    kernel::sched::cpu_struct_works = true;

    program_interrupts();

    if (!sched::cpus.push_back(i))
        panic("Could not add sched::CPU_Info struct to cpus vector\n");

    auto idle = proc::init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);

    auto t = proc::TaskGroup::create_for_task(i->idle_task);
    if (!t.success())
        panic("Failed to create task group for kernel: %i\n", t.result);
    proc::kernel_tasks = t.val;

    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    calculate_timer_frequency();

    // TODO: FP state
    detect_supported_extensions();
    init_interrupts();

    log::serial_logger.printf("Scheduling initialized\n");
}

extern "C" void smp_ap_entry()
{
    auto i = get_cpu_struct();
    serial_logger.printf("Kernel: Entered AP %i (phys %i)\n", i->cpu_id, i->cpu_physical_id); 

    kernel::loongarch64::paging::set_dmws();
    set_save0(i);
    program_interrupts();
    call_after_smp_entry();

    reschedule();

    serial_logger.printf("Kernel: AP %i entering userspace...\n", i->cpu_id);
}

void prepare_cpu(u32 phys_id)
{
    sched::CPU_Info *i = new sched::CPU_Info();
    if (!i)
        panic("Could not allocate sched::CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->cpu_physical_id  = phys_id;

    i->cpu_id = sched::cpus.size();
    if (!sched::cpus.push_back(i))
        panic("Could not add sched::CPU_Info struct to cpus vector\n");

    auto idle = proc::init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);

    assert(proc::kernel_tasks);
    if (auto t = proc::kernel_tasks->atomic_register_task(i->idle_task); t)
        panic("Failed to add idle task to the kernel process group: %i\n", t);

    i->current_task = i->idle_task;

    log::serial_logger.printf("Initialized CPU %u (phys %u)\n", i->cpu_id, i->cpu_physical_id);
}

void halt() { asm volatile("idle 0"); }

static bool have_online_capable_bit = false;
static void setup_online_capable()
{
    struct acpi_fadt *fadt;
    auto res = uacpi_table_fadt(&fadt);
    if (res != UACPI_STATUS_OK)
        panic("No FADT!");

    have_online_capable_bit = (fadt->hdr.revision > 6) or 
        (fadt->hdr.revision == 6 and fadt->fadt_minor_verison >= 3);
}

static bool cpu_usable(u32 flags)
{
    if (flags & ACPI_PIC_ENABLED)
        return true;

    if (!have_online_capable_bit)
        return false;

    return flags & ACPI_PIC_ONLINE_CAPABLE;
}

void init_smp_acpi()
{
    setup_online_capable();

    auto c = sched::get_cpu_struct();
    auto my_phys_id = c->cpu_physical_id;

    uacpi_table madt;
    auto res = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);
    if (res != UACPI_STATUS_OK) {
        return;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&madt);
    });

    struct Ctx {
        pmos::containers::set<u32> physical_ids;
    } ctx;


    if (ctx.physical_ids.insert_noexcept(my_phys_id).first == ctx.physical_ids.end())
        panic("Failed to allocate memory for my physical id");

    res = uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), [](auto c, auto hdr) -> uacpi_iteration_decision {
        Ctx &ctx = *(Ctx *)c;
        if (hdr->type == ACPI_MADT_ENTRY_TYPE_CORE_PIC) {
            acpi_madt_core_pic *cpic = (acpi_madt_core_pic *)hdr;

            if (!cpu_usable(cpic->flags))
                return UACPI_ITERATION_DECISION_CONTINUE;

            if (cpic->id == 0xFFFFFFFF)
                return UACPI_ITERATION_DECISION_CONTINUE;

            auto phys_id = cpic->id;

            auto res = ctx.physical_ids.insert_noexcept(phys_id);
            if (res.first == ctx.physical_ids.end())
                panic("Couldn't allocate memory for set in CPU initialization");

            if (!res.second)
                return UACPI_ITERATION_DECISION_CONTINUE;

            prepare_cpu(phys_id);
        }
        return UACPI_ITERATION_DECISION_CONTINUE;
    }, (void *)&ctx);
    if (res != UACPI_STATUS_OK)
        panic("uacpi_for_each_subtable error");
}

extern "C" void ap_bringup_trampoline();
extern phys_addr_t kernel_phys_base;
extern u8 _kernel_start;

static void mailbox_send(u32 cpu_phys, unsigned idx, u32 data)
{
    u64 value = 0x80000000;
    assert(idx < 8);
    value |= idx << 2;
    value |= cpu_phys << 16;
    value |= (u64)data << 32;

    iocsr_write64(value, iocsr::IPI_MAIL_SEND);
}

static void mailbox_send_u64(u32 cpu_phys, unsigned idx, u64 data)
{
    mailbox_send(cpu_phys, idx*2 + 1, data >> 32);
    mailbox_send(cpu_phys, idx*2, data);
}

void ipi_send(u32 cpu, u32 vector);

void start_aps()
{
    phys_addr_t phys_addr = kernel_phys_base + ((u8 *)&ap_bringup_trampoline - &_kernel_start);

    for (size_t i = 1; i < cpus.size(); ++i) {
        auto cpu = cpus[i];
        log::serial_logger.printf("Starting AP 0x%x (%x)\n", cpu->cpu_id, cpu->cpu_physical_id);

        mailbox_send_u64(cpu->cpu_physical_id, 0, phys_addr);
        mailbox_send_u64(cpu->cpu_physical_id, 1, (ulong)cpu);
        ipi_send(cpu->cpu_physical_id, 0);
    }
}

void init_smp()
{
    init_smp_acpi();

    start_aps();
}