#include <sched/sched.hh>
using namespace kernel::sched;

void CPU_Info::ipi_get_attention()
{
    panic("IPI get attention not implemented for m68k");
}

void CPU_Info::ipi_reschedule()
{
    panic("IPI reschedule not implemented for m68k");
}

void CPU_Info::ipi_cpu_park()
{
    panic("IPI reschedule not implemented for m68k");
}

void CPU_Info::ipi_tlb_shootdown()
{
    panic("IPI reschedule not implemented for m68k");
}