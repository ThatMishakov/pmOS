#include <sched/sched.hh>

using namespace kernel;

sched::CPU_Info *sched::get_cpu_struct()
{
    sched::CPU_Info *i;
    asm("mrs %0, tpidr_el1" : "=r"(i));
    return i;
}

void halt() { asm volatile("wfi"); }

extern "C" void print_stack_trace()
{};

void sched::CPU_Info::ipi_tlb_shootdown()
{
    assert(false);
}