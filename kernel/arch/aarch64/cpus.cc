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
DEFINE_SYSREG(pan)
DEFINE_SYSREG(id_aa64mmfr0_el1)
DEFINE_SYSREG(id_aa64mmfr1_el1)
DEFINE_SYSREG(id_aa64mmfr2_el1)
DEFINE_SYSREG(id_aa64pfr0_el1)
DEFINE_SYSREG(id_aa64pfr1_el1)
DEFINE_SYSREG(id_aa64pfr2_el1)
DEFINE_SYSREG(cpacr_el1)
DEFINE_SYSREG(S3_0_C1_C2_0) // ZCR_EL1
DEFINE_SYSREG(S3_3_C4_C4_0)
DEFINE_SYSREG(S3_3_C4_C4_1)
DEFINE_SYSREG(S3_3_C4_C4_2)

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

enum FPClass {
    None,
    FP,
    NEON,
    SVE,
};
FPClass system_fp_class = FPClass::None;
size_t sve_vector_length = 0;

size_t vector_data_length()
{
    switch (system_fp_class) {
    case FPClass::None:
        return 0;
    case FPClass::FP:
        return 32*8;
    case FPClass::NEON:
        return 32*16;
    case FPClass::SVE:
        return sve_vector_length*32 + 17*sve_vector_length/8;
    }
    return 0;
}

bool use_fpmr = false;


namespace {

void save_fp_registers(u64 *state)
{
    asm volatile(
        ".arch_extension fp\n\t"
        "stp d0, d1, [%0, #8*0]\n\t"
        "stp d2, d3, [%0, #8*2]\n\t"
        "stp d4, d5, [%0, #8*4]\n\t"
        "stp d6, d7, [%0, #8*6]\n\t"
        "stp d8, d9, [%0, #8*8]\n\t"
        "stp d10, d11, [%0, #8*10]\n\t"
        "stp d12, d13, [%0, #8*12]\n\t"
        "stp d14, d15, [%0, #8*14]\n\t"
        "stp d16, d17, [%0, #8*16]\n\t"
        "stp d18, d19, [%0, #8*18]\n\t"
        "stp d20, d21, [%0, #8*20]\n\t"
        "stp d22, d23, [%0, #8*22]\n\t"
        "stp d24, d25, [%0, #8*24]\n\t"
        "stp d26, d27, [%0, #8*26]\n\t"
        "stp d28, d29, [%0, #8*28]\n\t"
        "stp d30, d31, [%0, #8*30]\n\t"
        ".arch_extension nofp"   
    : : "r" (state) : "memory" );
}

void restore_fp_registers(u64 *state)
{
    asm volatile(
        ".arch_extension fp\n\t"
        "ldp d0, d1, [%0, #8*0]\n\t"
        "ldp d2, d3, [%0, #8*2]\n\t"
        "ldp d4, d5, [%0, #8*4]\n\t"
        "ldp d6, d7, [%0, #8*6]\n\t"
        "ldp d8, d9, [%0, #8*8]\n\t"
        "ldp d10, d11, [%0, #8*10]\n\t"
        "ldp d12, d13, [%0, #8*12]\n\t"
        "ldp d14, d15, [%0, #8*14]\n\t"
        "ldp d16, d17, [%0, #8*16]\n\t"
        "ldp d18, d19, [%0, #8*18]\n\t"
        "ldp d20, d21, [%0, #8*20]\n\t"
        "ldp d22, d23, [%0, #8*22]\n\t"
        "ldp d24, d25, [%0, #8*24]\n\t"
        "ldp d26, d27, [%0, #8*26]\n\t"
        "ldp d28, d29, [%0, #8*28]\n\t"
        "ldp d30, d31, [%0, #8*30]\n\t"
        ".arch_extension nofp"   
    : : "r" (state) : "memory" );
}

void save_neon_registers(u64 *state)
{
    asm volatile(
        ".arch_extension simd\n\t"
        "stp q0, q1, [%0, #16*0]\n\t"
        "stp q2, q3, [%0, #16*2]\n\t"
        "stp q4, q5, [%0, #16*4]\n\t"
        "stp q6, q7, [%0, #16*6]\n\t"
        "stp q8, q9, [%0, #16*8]\n\t"
        "stp q10, q11, [%0, #16*10]\n\t"
        "stp q12, q13, [%0, #16*12]\n\t"
        "stp q14, q15, [%0, #16*14]\n\t"
        "stp q16, q17, [%0, #16*16]\n\t"
        "stp q18, q19, [%0, #16*18]\n\t"
        "stp q20, q21, [%0, #16*20]\n\t"
        "stp q22, q23, [%0, #16*22]\n\t"
        "stp q24, q25, [%0, #16*24]\n\t"
        "stp q26, q27, [%0, #16*26]\n\t"
        "stp q28, q29, [%0, #16*28]\n\t"
        "stp q30, q31, [%0, #16*30]\n\t"
        ".arch_extension nosimd"   
    : : "r" (state) : "memory" );
}
void restore_neon_registers(u64 *state)
{
    asm volatile(
        ".arch_extension simd\n\t"
        "ldp q0, q1, [%0, #16*0]\n\t"
        "ldp q2, q3, [%0, #16*2]\n\t"
        "ldp q4, q5, [%0, #16*4]\n\t"
        "ldp q6, q7, [%0, #16*6]\n\t"
        "ldp q8, q9, [%0, #16*8]\n\t"
        "ldp q10, q11, [%0, #16*10]\n\t"
        "ldp q12, q13, [%0, #16*12]\n\t"
        "ldp q14, q15, [%0, #16*14]\n\t"
        "ldp q16, q17, [%0, #16*16]\n\t"
        "ldp q18, q19, [%0, #16*18]\n\t"
        "ldp q20, q21, [%0, #16*20]\n\t"
        "ldp q22, q23, [%0, #16*22]\n\t"
        "ldp q24, q25, [%0, #16*24]\n\t"
        "ldp q26, q27, [%0, #16*26]\n\t"
        "ldp q28, q29, [%0, #16*28]\n\t"
        "ldp q30, q31, [%0, #16*30]\n\t"
        ".arch_extension nosimd"   
    : : "r" (state) : "memory" );
}

void save_sve_registers(u64 *state)
{
    panic("SVE not handled correctly");
}
void restore_sve_registers(u64 *state)
{
    panic("SVE not handled correctly");
}

} // namespace

void proc::TaskDescriptor::after_task_switch()
{
    u64 cpacr = 0;
    if (fp_class >= FPClass::FP)
        cpacr |= 0b11 << 20; // Enable FP/NEON
    if (fp_class >= FPClass::SVE)
        cpacr |= 0b11 << 16; // Enable SVE
    msr<cpacr_el1>(cpacr);
    isb();

    if (fp_class == FPClass::None)
        return;

    switch(fp_class) {
    case FPClass::None:
        break;
    case FPClass::FP:
        restore_fp_registers(fp_state.get());
        break;
    case FPClass::NEON:
        restore_neon_registers(fp_state.get());
        break;
    case FPClass::SVE:
        restore_sve_registers(fp_state.get());
        break;
    }

    msr<S3_3_C4_C4_0>(fpcr);
    msr<S3_3_C4_C4_1>(fpsr);
    if (use_fpmr)
        msr<S3_3_C4_C4_2>(fpmr);
}

void proc::TaskDescriptor::before_task_switch()
{
    switch(fp_class) {
    case FPClass::None:
        return;
    case FPClass::FP:
        save_fp_registers(fp_state.get());
        break;
    case FPClass::NEON:
        save_neon_registers(fp_state.get());
        break;
    case FPClass::SVE:
        save_sve_registers(fp_state.get());
        break;
    }

    fpcr = mrs<S3_3_C4_C4_0>();
    fpsr = mrs<S3_3_C4_C4_1>();
    if (use_fpmr)
        fpmr = mrs<S3_3_C4_C4_2>();
}

kresult_t proc::TaskDescriptor::init_fp_state()
{
    if (system_fp_class == FPClass::None)
        return 0;

    auto data_length = vector_data_length();
    if (data_length == 0)
        return 0;

    size_t u64_count = data_length / sizeof(u64);
    if (data_length % sizeof(u64))
        u64_count++;

    fp_state = klib::unique_ptr<u64[]>(new u64[u64_count]);
    if (!fp_state)
        return -ENOMEM;

    memset(fp_state.get(), 0, data_length);

    // Enable FP/NEON, let SVE be enabled lazily on first use
    if (system_fp_class < FPClass::SVE)
        fp_class = system_fp_class;
    else
        fp_class = FPClass::NEON;

    return 0;
}

bool use_pan = false;
bool use_ttcnp = false;

constexpr u64 PAN_MASK = 0xf << 20;

constexpr u64 CnP_MASK = 0xf << 0;

void early_detect_cpu_features()
{
    // uint64_t id_aa64mmfr0_el1 = mrs<id_aa64mmfr0_el1>();

    auto id_aa64mmfr1 = mrs<id_aa64mmfr1_el1>();
    use_pan = id_aa64mmfr1 & PAN_MASK;

    auto id_aa64mmfr2 = mrs<id_aa64mmfr2_el1>();
    use_ttcnp = id_aa64mmfr2 & CnP_MASK;

    auto id_aa64pfr0 = mrs<id_aa64pfr0_el1>();
    if (((id_aa64pfr0 >> 16) & 0xf) != 0xf)
        system_fp_class = FPClass::FP;

    if (((id_aa64pfr0 >> 20) & 0xf) != 0xf)
        system_fp_class = FPClass::NEON;

    if (((id_aa64pfr0 >> 32) & 0xf) == 0b0001) {
        system_fp_class = FPClass::SVE;

        // Enable SVE
        msr<cpacr_el1>(mrs<cpacr_el1>() | (0b11 << 16));
        isb();

        // ZCR_EL1
        msr<S3_0_C1_C2_0>(0xf);
        ulong vl;
        asm volatile(
            ".arch_extension sve\n\t"
            "rdvl %0, 1\n\t"
            ".arch_extension nosve"
            : "=r"(vl));
        sve_vector_length = vl;
    }

    auto id_aa64pfr2 = mrs<id_aa64pfr2_el1>();
    if (((id_aa64pfr2 >> 32) & 0xf) == 0b0001)
        use_fpmr = true;
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
        isb();
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
        timer_interrupt = gtdt->el1_virtual_gsiv;
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

    assert(timer_interrupt >= 16 && timer_interrupt < 32);
    auto timer_ppi = timer_interrupt - 16;
    arm::interrupts::ppi_handlers[timer_ppi] = []() {
        msr<cntv_ctl_el0>(2);
        isb();
        sched::cpu_timer_interrupt();
    };
}

void set_cpu_struct(CPU_Info *i)
{
    msr<tpidr_el1>((ulong)i);
}

extern "C" void exception_vectors();

void program_cpu_stuff(sched::CPU_Info *)
{
    msr<vbar_el1>((ulong)exception_vectors);

    // The right stack gets loaded when returning to userspace
    // assert(mrs<spsel>() == 0);
    // msr<sp_el1>((ulong)i->kernel_stack_top);
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

    program_cpu_stuff(i);

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
    arm::interrupts::do_cpu_interrupts_init();

    enable_timer_interrupt();
}

void init_smp()
{
    // init_smp_acpi();

    memory::init_rcu();
    other_cpus_online = true;

    // start_aps();
}