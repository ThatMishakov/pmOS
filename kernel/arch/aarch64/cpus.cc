#include <sched/sched.hh>

using namespace kernel::sched;

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

void init_scheduling(u64 cpu_id)
{
    panic("Init scheduling on aarch64 todo!");
}

void init_smp()
{
    // TODO as well...
}