#include <csr.hh>
#include <interrupts.hh>
#include <kern_logger/kern_logger.hh>
#include <loongarch_asm.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>

using namespace kernel;

extern "C" void isr();

namespace kernel::sched
{
extern bool cpu_struct_works;
}

register sched::CPU_Info *cpu_struct __asm__("$tp");
static void set_cpu_struct(sched::CPU_Info *i) { cpu_struct = i; }

sched::CPU_Info *sched::get_cpu_struct() { return cpu_struct; }

void set_save0(sched::CPU_Info *i) { csrwr<loongarch::csr::SAVE0>(i); }

void program_interrupts()
{
    csrwr<loongarch::csr::ECFG>(0x1fff);
    csrwr<loongarch::csr::EENTRY>(isr);
}

void detect_supported_extensions();
void init_interrupts();

void init_scheduling(u64 cpu_id)
{
    sched::CPU_Info *i = new sched::CPU_Info();
    if (!i)
        panic("Could not allocate sched::CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->cpu_physical_id  = cpu_id;

    set_cpu_struct(i);
    set_save0(i);

    kernel::sched::cpu_struct_works = true;

    program_interrupts();

    if (!sched::cpus.push_back(i))
        panic("Could not add sched::CPU_Info struct to cpus vector\n");

    auto idle = proc::init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);

    auto t = proc::TaskGroup::create_for_task(i->idle_task);
    if (!t.success())
        panic("Failed to create task group for kernel: %i\n", t.result);
    proc::kernel_tasks = t.val;

    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    // initialize_timer();

    // TODO: FP state
    detect_supported_extensions();
    init_interrupts();

    log::serial_logger.printf("Scheduling initialized\n");
}

void halt() { asm volatile("idle 0"); }