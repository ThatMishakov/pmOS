#include <sched/sched.hh>
#include <csr.hh>
#include <loongarch_asm.hh>
#include <processes/tasks.hh>
#include <kern_logger/kern_logger.hh>
#include <interrupts.hh>

using namespace kernel;

extern "C" void isr();

extern bool cpu_struct_works;
register CPU_Info *cpu_struct __asm__("$tp");
void set_cpu_struct(CPU_Info *i)
{
    cpu_struct = i;
}

extern "C" CPU_Info *get_cpu_struct()
{
    return cpu_struct;
}

void set_save0(CPU_Info *i)
{

    csrwr<loongarch::csr::SAVE0>(i);
}

void program_interrupts()
{
    csrwr<loongarch::csr::ECFG>(TIMER_INT_MASK);
    csrwr<loongarch::csr::EENTRY>(isr);
}

void init_scheduling(u64 cpu_id)
{
    CPU_Info *i         = new CPU_Info();
    if (!i)
        panic("Could not allocate CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->cpu_physical_id = cpu_id;

    set_cpu_struct(i);
    set_save0(i);

    cpu_struct_works = true;

    program_interrupts();

    if (!cpus.push_back(i))
        panic("Could not add CPU_Info struct to cpus vector\n");

    auto idle = init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);
    
    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    //initialize_timer();

    // TODO: FP state
    // TODO: Interrupts

    serial_logger.printf("Scheduling initialized\n");
}

void halt() { asm volatile("idle 0"); }