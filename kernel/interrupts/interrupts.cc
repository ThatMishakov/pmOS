#include "interrupts.hh"
#include <utils.hh>
#include "gdt.hh"
#include <memory/palloc.hh>
#include <memory/malloc.hh>
#include "exceptions_managers.hh"
#include "asm.hh"
#include <processes/sched.hh>
#include <processes/syscalls.hh>
#include <misc.hh>
#include "apic.hh"

void set_idt()
{
    IDT_descriptor desc = {sizeof(IDT) - 1, (u64)&k_idt};
    loadIDT(&desc);
}

void init_IDT()
{
    fill_idt();
    init_apic();

    set_idt();
}

int tss_index = 0;

void init_kernel_stack()
{
    int tss_i = tss_index++;

    CPU_Info* s = get_cpu_struct();
    s->kernel_stack = (Stack*)palloc(sizeof(Stack)/4096);
    kernel_gdt.SSD_entries[tss_i] = System_Segment_Descriptor((u64) calloc(1,sizeof(TSS)), sizeof(TSS), 0x89, 0x02);
    kernel_gdt.SSD_entries[tss_i].tss()->ist1 = (u64)s->kernel_stack + sizeof(Stack);
    loadTSS(0x28 + tss_i*0x10);
}

void init_interrupts()
{
    init_IDT();
    init_kernel_stack();
    asm("sti");
    discover_apic_freq();
}

extern "C" void interrupt_handler(u64 intno, u64 err, Interrupt_Stackframe* int_s)
{
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
        case 0x8: 
            t_print_bochs("!!! Double fault (DF) [ABORT]\n");
            halt();
            break;
        case 0xE:
            pagefault_manager(err, int_s);
            break;
        case 0xD:
            t_print_bochs("!!! General Protection Fault (GP) error %h\n", err);
            halt();
            break;
        default:
            t_print_bochs("!!! Unknown exception %h. Not currently handled.\n", intno);
            halt();
            break;
    }
    else
    {
        switch(intno) {
        case APIC_SPURIOUS_INT:
            //t_print_bochs("Notice: Recieved spurious int\n");
            break;
        case APIC_TMR_INT:
            sched_periodic();
            apic_eoi();
            break;
        default:
            t_print_bochs("!!! Recieved unknown interrupt %h\n", intno);
            break;
        }
    }
}