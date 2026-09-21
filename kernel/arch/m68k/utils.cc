#include <sched/sched.hh>
#include <interrupts/stack.hh>
#include <utils.hh>

using namespace kernel::sched;

CPU_Info *cpu_struct = nullptr;

kernel::sched::CPU_Info *kernel::sched::get_cpu_struct() {
    return cpu_struct;
}

void halt()
{
    asm("stop #0x2000");
}

void arch_specific_park_stop()
{
    hcf();
}

void arch_specific_park_pre_offline()
{
}

void printc(int)
{
    panic("printc not implemented for m68k");
}

extern "C" void print_stack_trace()
{
}