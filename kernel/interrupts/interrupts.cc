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

void set_idt()
{
    IDT_descriptor desc = {sizeof(IDT) - 1, (u64)&k_idt};
    loadIDT(&desc);
}

void init_IDT()
{
    fill_idt();
    enable_apic();

    set_idt();
}

void init_interrupts()
{
    init_IDT();
    asm("sti");
    discover_apic_freq();
}

void programmable_interrupt(u32 intno)
{
    //t_print_bochs("Interrupt %i\n", intno);

    Kernel_Message_Interrupt kmsg = {KERNEL_MSG_INTERRUPT, intno, get_lapic_id()};
    send_message_system(KERNEL_MSG_INT_START + intno, reinterpret_cast<char*>(&kmsg), sizeof(kmsg));

    smart_eoi(intno);
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
        case 0x6: 
            t_print_bochs("!!! Invalid op-code (UD) instr %h\n", int_s->rip);
            halt();
            break;
        case 0x07:
            //t_print_bochs("!!! Device Not Available (NM) PID %h instr %h -> restoring SSE state\n", t->pid, int_s->rip);
            sse_exception_manager();
            break;
        case 0x8: 
            t_print_bochs("!!! Double fault (DF) [ABORT]\n");
            halt();
            break;
        case 0xE:
            //t_print_bochs("!!! Page Fault (PF) error %h PID %i RIP %h\n", err, get_cpu_struct()->current_task->pid, int_s->rip);
            pagefault_manager(err, int_s);
            break;
        case 0xD:
            //t_print_bochs("!!! General Protection Fault (GP) error %h\n", err);
            t_print_bochs("!!! General Protection Fault (GP) error (segment) %h PID %i RIP %h CS %h... Killing the process\n", err, get_cpu_struct()->current_task->pid, int_s->rip, int_s->cs);
            syscall_exit(4, 0);
            break;
        case 0xC:
            t_print_bochs("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", err, int_s->rip, int_s->rsp);
            t_print("!!! Stack-Segment Fault error %h RIP %h RSP %h\n", err, int_s->rip, int_s->rsp);
            syscall_exit(4, 0);
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
        smart_eoi(intno);
    }
}

void (*return_table[2])(void) = {
    ret_from_interrupt,
    ret_from_syscall,
};