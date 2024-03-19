#include "interrupts.hh"
#include <utils.hh>
#include "gdt.hh"
#include <memory/palloc.hh>
#include <memory/malloc.hh>
#include "exceptions_managers.hh"
#include <x86_asm.hh>
#include <sched/sched.hh>
#include <processes/syscalls.hh>
#include <misc.hh>
#include "apic.hh"
#include <kernel/messaging.h>
#include "idt.hh"
#include <cpus/ipi.hh>

void set_idt()
{
    IDTR desc = {sizeof(IDT) - 1, &k_idt};
    loadIDT(&desc);
}

void init_idt()
{
    enable_apic();

    set_idt();
}

void init_interrupts()
{
    init_idt();
    discover_apic_freq();
}

extern "C" void timer_interrupt()
{
    // apic_eoi was always at the end, but I think it needs to be at the beginning so that the interrupts are not lost
    // Not sure about it and don't remember what it was doing exactly
    apic_eoi();

    sched_periodic();
}

/*
extern "C" void interrupt_handler()
{
    const klib::shared_ptr<TaskDescriptor>& t = get_cpu_struct()->current_task;
    u64 intno = t->regs.intno;
    u64 err = t->regs.int_err;
    Interrupt_Stackframe* int_s = &t->regs.e;

    //t_print_bochs("Int %h error %h pid %h rip %h cs %h\n", intno, err, t->pid, t->regs.e.rip, t->regs.e.cs);

    if (intno < 32)
    switch (intno) {
        case 0x0: 
            t_print("!!! Division by zero (DE)\n");
            break;
        case 0x4:
            t_print("!!! Overflow (OF)\n");
            break;
        case 0x8: 
            t_print_bochs("!!! Double fault (DF) [ABORT]\n");
            halt();
            break;
        default:
            t_print_bochs("!!! Unknown exception %h. Not currently handled.\n", intno);
            halt();
            break;
    }
    else if (intno < 48) {
        // Spurious PIC interrupt
    } else if (intno < 0xf0) {
        programmable_interrupt(intno);
    } else if (intno == 0xfb) {
        sched_periodic();
        // smart_eoi(intno);
    }
}
*/

void (*return_table[4])(void) = {
    ret_from_interrupt,
    ret_from_syscall,
    ret_from_sysenter,
    ret_repeat_syscall,
};