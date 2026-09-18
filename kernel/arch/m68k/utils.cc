#include <sched/sched.hh>
#include <interrupts/stack.hh>

using namespace kernel::sched;

constexpr ulong STACK_MASK = STACK_SIZE - 1;

kernel::sched::CPU_Info *kernel::sched::get_cpu_struct() {
    register uintptr_t sp asm("%sp");
    uintptr_t sp_val = (sp + STACK_MASK) & ~STACK_MASK;
    return *(kernel::sched::CPU_Info **)(sp_val - sizeof(kernel::sched::CPU_Info *));
}

void halt()
{
    asm("stop #$2000");
}