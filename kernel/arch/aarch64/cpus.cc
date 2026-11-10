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
