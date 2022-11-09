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

void init_IDT()
{
    fill_idt();
    mask_PIC();

    IDT_descriptor desc = {sizeof(IDT) - 1, (u64)&k_idt};
    loadIDT(&desc);
}

void init_kernel_stack()
{
    CPU_Info* s = get_cpu_struct();
    s->kernel_stack = (Stack*)palloc(sizeof(Stack)/4096);
    kernel_gdt.SSD_entries[0] = System_Segment_Descriptor((u64) calloc(1,sizeof(TSS)), sizeof(TSS), 0x89, 0x02);
    kernel_gdt.SSD_entries[0].tss()->ist1 = (u64)s->kernel_stack + sizeof(Stack);
    loadTSS(0x28);
}

void init_interrupts()
{
    init_IDT();
    init_kernel_stack();
    asm("sti");
}

extern "C" void interrupt_handler(u64 intno, u64 err, Interrupt_Stackframe* int_s)
{
    switch (intno) {
        case 0x0: 
            t_print("!!! Division by zero (DE)\n");
            break;
        case 0x4:
            t_print("!!! Overflow (OF)\n");
            break;
        case 0x6: 
            t_print("!!! Invalid op-code (UD) instr %h\n", int_s->rip);
            halt();
            break;
        case 0x8: 
            t_print("!!! Double fault (DF) [ABORT]\n");
            halt();
            break;
        case 0xE:
            pagefault_manager(err, int_s);
            break;
        case 0xD:
            t_print("!!! General Protection Fault (GP) error %h\n", err);
            halt();
            break;
        default:
            t_print("!!! Unknown interrupt %h. Not currently handled.\n", intno);
            halt();
            break;
    }
}