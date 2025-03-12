#include <sched/sched.hh>

extern "C" void isr();

extern bool cpu_struct_works;
register CPU_Info *cpu_struct __asm__("$tp");
void set_cpu_struct(CPU_Info *i)
{
    cpu_struct = i;
}

void set_save0(CPU_Info *i)
{
    asm volatile("csrwr %0, 0x30" :: "r"(i) : "memory");
}

void program_interrupts()
{
    asm volatile("csrwr %0, 0x4" :: "r"(0xfff) : "memory");
    asm volatile("csrwr %0, 0xC" :: "r"(isr) : "memory");
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

    //initialize_timer();

    // TODO: FP state
    // TODO: Interrupts
}