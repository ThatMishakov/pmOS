#include <sched/sched.hh>
using namespace kernel::sched;

void CPU_Info::ipi_get_attention()
{
    assert(false && "IPI get attention not implemented for m68k");
}

void CPU_Info::ipi_reschedule()
{
    assert(false && "IPI reschedule not implemented for m68k");
}

void CPU_Info::ipi_cpu_park()
{
    assert(false && "IPI cpu park not implemented for m68k");
}

void CPU_Info::ipi_tlb_shootdown()
{
    assert(false && "IPI tlb shootdown not implemented for m68k");
}

void init_smp()
{
}