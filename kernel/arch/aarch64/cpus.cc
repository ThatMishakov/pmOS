#include <sched/sched.hh>
#include <memory/paging.hh>
#include <kern_logger/kern_logger.hh>
#include <processes/tasks.hh>
#include <arm_asm.hh>
#include <interrupts/interrupts.hh>
#include <time/timers.hh>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <pmos/utility/scope_guard.hh>


DEFINE_SYSREG(tpidr_el1)
DEFINE_SYSREG(vbar_el1)
DEFINE_SYSREG(cntfrq_el0)
DEFINE_SYSREG(cntvct_el0)
DEFINE_SYSREG(cntv_cval_el0)
DEFINE_SYSREG(cntv_ctl_el0)
DEFINE_SYSREG(currentel)
DEFINE_SYSREG(hcr_el2)

using namespace kernel::sched;
using namespace kernel;
using namespace kernel::arm;

void CPU_Info::ipi_cpu_park()
{
    panic("IPI CPU park not implemented");
}

void CPU_Info::ipi_reschedule()
{
    panic("IPI CPU reschedule not implemented");
}

void CPU_Info::ipi_get_attention()
{
    panic("IPI CPU get attention not implemented");
}

bool use_pan = false;
bool use_ttcnp = false;

constexpr u64 PAN_MASK = 0xf << 20;

constexpr u64 CnP_MASK = 0xf << 0;

void early_detect_cpu_features()
{
    uint64_t id_aa64mmfr1_el1;
    asm volatile("mrs %0, id_aa64mmfr1_el1" : "=r"(id_aa64mmfr1_el1));
    use_pan = id_aa64mmfr1_el1 & PAN_MASK;

    uint64_t id_aa64mmfr2_el1;
    asm volatile("mrs %0, id_aa64mmfr2_el1" : "=r"(id_aa64mmfr2_el1));
    use_ttcnp = id_aa64mmfr2_el1 & CnP_MASK;
}

static FreqFraction timer_freq;
static FreqFraction timer_period;

struct ARMTimeSource final: kernel::time::TimeSource {
    virtual u64 get_absolute_time() const override
    {
        return timer_period * mrs<cntvct_el0>();
    }

    virtual const char *name() const override
    {
        return "ARM Generic Timer";
    }

    virtual void init_as_main() override {}
} arm_time_source;

struct ARMGenericTimer final: kernel::time::LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) override
    {
        u64 wanted_ticks = timer_freq * deadline_nanoseconds;
        msr<cntv_cval_el0>(wanted_ticks);
        msr<cntv_ctl_el0>(1);
    }

    virtual void cancel_deadline() override
    {
        msr<cntv_ctl_el0>(2);
    }

    virtual void init_as_main() override {}

    virtual const char *name() const override
    {
        return "ARM Generic Timer";
    }
};
ARMGenericTimer arm_generic_timer;

constexpr u64 EL2_VHE_MASK = 1ull << 34;

unsigned timer_interrupt = 0;

bool init_timer_acpi()
{
    uacpi_table m;
    auto res = uacpi_table_find_by_signature("GTDT", &m);
    if (res != UACPI_STATUS_OK) {
        log::serial_logger.printf("Couldn't get GTDT table...\n");
        return false;
    }
    auto guard = pmos::utility::make_scope_guard([&]{
        uacpi_table_unref(&m);
    });

    acpi_gtdt *gtdt = (acpi_gtdt *)m.ptr;

    auto current_el = (mrs<currentel>() >> 2) & 0x3;
    if (current_el == 1) {
        timer_interrupt = gtdt->el1_non_secure_gsiv;
    } else if (current_el == 2) {
        if (!(mrs<hcr_el2>() & EL2_VHE_MASK))
            panic("Kernel in EL2 w/o VHE, not (yet) supported");

        if (gtdt->hdr.revision < 3)
            panic("GTDT revision %u", gtdt->hdr.revision);

        timer_interrupt = gtdt->el2_virtual_gsiv;
        // Mode and polarity don't matter here since it's a PPI
    } else {
        panic("Kernel in EL%u", current_el);
    }

    return true;
}

void init_timer()
{
    auto counter_freq = mrs<cntfrq_el0>();
    timer_freq   = computeFreqFraction(counter_freq, 1'000'000'000);
    timer_period = computeFreqFraction(1'000'000'000, counter_freq);

    log::serial_logger.printf("Timer frequency: %llu Hz\n", counter_freq);

    kernel::time::kernel_timesource = &arm_time_source;
    kernel::time::kernel_local_timer = &arm_generic_timer;

    bool success = init_timer_acpi();
    if (!success)
        panic("Failed to initialize timer with ACPI, DTs not implemented");
}

void set_cpu_struct(CPU_Info *i)
{
    msr<tpidr_el1>((ulong)i);
}

extern "C" void exception_vectors();

void program_cpu_stuff()
{
    msr<vbar_el1>((ulong)exception_vectors);
}

// TODO: This is bad and should be abstracted away
extern int kernel_pt_generation;
extern int kernel_pt_active_cpus_count[2];

void enable_timer_interrupt()
{
    msr<cntv_ctl_el0>(2);
    isb();
    arm::interrupts::ppi_enable(timer_interrupt);
}

void init_scheduling(u64 cpu_id)
{
    CPU_Info *i = new CPU_Info();
    if (!i)
        panic("Could not allocate CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->mpidr_el1        = mpidr_el1();
    set_cpu_struct(i);

    cpu_struct_works = true;

    program_cpu_stuff();

    if (!cpus.push_back(i))
        panic("Could not add sched::CPU_Info struct to cpus vector\n");

    log::serial_logger.printf("Initializing idle task\n");

    auto idle = proc::init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);

    auto t = proc::TaskGroup::create_for_task(i->idle_task);
    if (!t.success())
        panic("Failed to create task group for kernel: %i\n", t.result);
    proc::kernel_tasks = t.val;

    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    i->kernel_pt_generation = kernel::paging::kernel_pt_generation;
    __atomic_add_fetch(&kernel_pt_active_cpus_count[i->kernel_pt_generation], 1, __ATOMIC_RELAXED);

    arm::interrupts::init_interrupt_controllers();

    log::serial_logger.printf("Scheduling initialized\n");

    init_timer();
    enable_timer_interrupt();
}

void init_smp()
{
    // init_smp_acpi();

    memory::init_rcu();
    other_cpus_online = true;

    // start_aps();
}