#include <sched/sched.hh>
#include <interrupts/stack.hh>
#include <utils.hh>

using namespace kernel::sched;

constexpr ulong STACK_MASK = STACK_SIZE - 1;

kernel::sched::CPU_Info *kernel::sched::get_cpu_struct() {
    register uintptr_t sp asm("%sp");
    uintptr_t sp_val = (sp + STACK_MASK) & ~STACK_MASK;
    return *(kernel::sched::CPU_Info **)(sp_val - sizeof(kernel::sched::CPU_Info *));
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