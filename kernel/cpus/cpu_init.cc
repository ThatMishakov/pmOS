#include "cpu_init.hh"
#include <processes/sched.hh>
#include <asm.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/apic.hh>

void init_per_cpu()
{
    CPU_Info* c = new CPU_Info;
    write_msr(0xC0000101, (u64)c);
    init_idle();
}

extern "C" void cpu_start_routine()
{
    init_per_cpu();
    set_idt();
    init_kernel_stack();
    enable_apic();

    find_new_process();
    
    t_print_bochs("CPU started!\n");
}