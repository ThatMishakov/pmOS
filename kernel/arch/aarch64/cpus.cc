#include <sched/sched.hh>
#include <memory/paging.hh>
#include <kern_logger/kern_logger.hh>
#include <processes/tasks.hh>
#include <arm_asm.hh>
#include <interrupts/interrupts.hh>

using namespace kernel::sched;
using namespace kernel;
using namespace kernel::arm;

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

void set_cpu_struct(CPU_Info *i)
{
    asm volatile("msr tpidr_el1, %0" :: "r"(i) : "memory");
}

extern "C" void exception_vectors();

void program_cpu_stuff()
{
    asm volatile("msr vbar_el1, %0" :: "r"(exception_vectors) : "memory");
}

// TODO: This is bad and should be abstracted away
extern int kernel_pt_generation;
extern int kernel_pt_active_cpus_count[2];

void init_scheduling(u64 cpu_id)
{
    CPU_Info *i = new CPU_Info();
    if (!i)
        panic("Could not allocate CPU_Info struct\n");

    i->kernel_stack_top = i->kernel_stack.get_stack_top();
    i->mpidr_el1        = mpidr_el1();
    set_cpu_struct(i);

    cpu_struct_works = true;

    program_cpu_stuff();

    if (!cpus.push_back(i))
        panic("Could not add sched::CPU_Info struct to cpus vector\n");

    log::serial_logger.printf("Initializing idle task\n");

    auto idle = proc::init_idle(i);
    if (idle != 0)
        panic("Failed to initialize idle task: %i\n", idle);

    auto t = proc::TaskGroup::create_for_task(i->idle_task);
    if (!t.success())
        panic("Failed to create task group for kernel: %i\n", t.result);
    proc::kernel_tasks = t.val;

    i->current_task = i->idle_task;
    i->idle_task->page_table->apply_cpu(i);

    i->kernel_pt_generation = kernel::paging::kernel_pt_generation;
    __atomic_add_fetch(&kernel_pt_active_cpus_count[i->kernel_pt_generation], 1, __ATOMIC_RELAXED);

    arm::interrupts::init_interrupt_controllers();

    log::serial_logger.printf("Scheduling initialized\n");
}

void init_smp()
{
    panic("aarch64 init_smp stub\n");
}