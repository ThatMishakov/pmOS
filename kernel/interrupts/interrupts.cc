#include "interrupts.hh"
#include <utils.hh>
#include "gdt.hh"
#include <memory/palloc.hh>
#include <memory/malloc.hh>
#include "exceptions_managers.hh"
#include "asm.hh"
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
    asm("sti");
    discover_apic_freq();
}

void programmable_interrupt(u32 intno)
{
    // t_print_bochs("Interrupt %i\n", intno);

    Kernel_Message_Interrupt kmsg = {KERNEL_MSG_INTERRUPT, intno, get_lapic_id()};
    send_message_system(KERNEL_MSG_INT_START + intno, reinterpret_cast<char*>(&kmsg), sizeof(kmsg));

    apic_eoi();
}

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

void (*return_table[4])(void) = {
    ret_from_interrupt,
    ret_from_syscall,
    ret_from_sysenter,
    ref_from_exception,
};