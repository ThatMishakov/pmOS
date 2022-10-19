#include "interrupts.hh"
#include "utils.hh"
#include "gdt.hh"
#include "palloc.hh"
#include "malloc.hh"
#include "pagefault_manager.hh"
#include "asm.hh"
#include "sched.hh"
#include "syscalls.hh"
#include "misc.hh"

IDT k_idt = {};

Stack* kernel_stack;

void init_IDT()
{
    k_idt.entries[0] = Gate_Descriptor((u64)&isr0, IST, INTGATE);
    k_idt.entries[1] = Gate_Descriptor((u64)&isr1, IST, TRAPGATE);
    k_idt.entries[2] = Gate_Descriptor((u64)&isr2, IST, INTGATE);
    k_idt.entries[3] = Gate_Descriptor((u64)&isr3, IST, TRAPGATE);
    k_idt.entries[4] = Gate_Descriptor((u64)&isr4, IST, TRAPGATE);
    k_idt.entries[5] = Gate_Descriptor((u64)&isr5, IST, INTGATE);
    k_idt.entries[6] = Gate_Descriptor((u64)&isr6, IST, INTGATE);
    k_idt.entries[7] = Gate_Descriptor((u64)&isr7, IST, INTGATE);
    k_idt.entries[8] = Gate_Descriptor((u64)&isr8, IST, INTGATE);
    k_idt.entries[9] = Gate_Descriptor((u64)&isr9, IST, INTGATE);
    k_idt.entries[10] = Gate_Descriptor((u64)&isr10, IST, INTGATE);
    k_idt.entries[11] = Gate_Descriptor((u64)&isr11, IST, INTGATE);
    k_idt.entries[12] = Gate_Descriptor((u64)&isr12, IST, INTGATE);
    k_idt.entries[13] = Gate_Descriptor((u64)&isr13, IST, INTGATE);
    k_idt.entries[14] = Gate_Descriptor((u64)&isr14, IST, INTGATE);
    k_idt.entries[15] = Gate_Descriptor((u64)&isr15, IST, INTGATE);
    k_idt.entries[16] = Gate_Descriptor((u64)&isr16, IST, INTGATE);
    k_idt.entries[17] = Gate_Descriptor((u64)&isr17, IST, INTGATE);
    k_idt.entries[18] = Gate_Descriptor((u64)&isr18, IST, INTGATE);
    k_idt.entries[19] = Gate_Descriptor((u64)&isr19, IST, INTGATE);
    k_idt.entries[20] = Gate_Descriptor((u64)&isr20, IST, INTGATE);
    k_idt.entries[21] = Gate_Descriptor((u64)&isr21, IST, INTGATE);
    k_idt.entries[22] = Gate_Descriptor((u64)&isr22, IST, INTGATE);
    k_idt.entries[23] = Gate_Descriptor((u64)&isr23, IST, INTGATE);
    k_idt.entries[24] = Gate_Descriptor((u64)&isr24, IST, INTGATE);
    k_idt.entries[25] = Gate_Descriptor((u64)&isr25, IST, INTGATE);
    k_idt.entries[26] = Gate_Descriptor((u64)&isr26, IST, INTGATE);
    k_idt.entries[27] = Gate_Descriptor((u64)&isr27, IST, INTGATE);
    k_idt.entries[28] = Gate_Descriptor((u64)&isr28, IST, INTGATE);
    k_idt.entries[29] = Gate_Descriptor((u64)&isr29, IST, INTGATE);
    k_idt.entries[30] = Gate_Descriptor((u64)&isr30, IST, INTGATE);
    k_idt.entries[31] = Gate_Descriptor((u64)&isr31, IST, INTGATE);
    k_idt.entries[32] = Gate_Descriptor((u64)&isr32, IST, INTGATE);
    k_idt.entries[33] = Gate_Descriptor((u64)&isr33, IST, INTGATE);
    k_idt.entries[34] = Gate_Descriptor((u64)&isr34, IST, INTGATE);
    k_idt.entries[35] = Gate_Descriptor((u64)&isr35, IST, INTGATE);
    k_idt.entries[36] = Gate_Descriptor((u64)&isr36, IST, INTGATE);
    k_idt.entries[37] = Gate_Descriptor((u64)&isr37, IST, INTGATE);
    k_idt.entries[38] = Gate_Descriptor((u64)&isr38, IST, INTGATE);
    k_idt.entries[39] = Gate_Descriptor((u64)&isr39, IST, INTGATE);
    k_idt.entries[40] = Gate_Descriptor((u64)&isr40, IST, INTGATE);
    k_idt.entries[41] = Gate_Descriptor((u64)&isr41, IST, INTGATE);
    k_idt.entries[42] = Gate_Descriptor((u64)&isr42, IST, INTGATE);
    k_idt.entries[43] = Gate_Descriptor((u64)&isr43, IST, INTGATE);
    k_idt.entries[44] = Gate_Descriptor((u64)&isr44, IST, INTGATE);
    k_idt.entries[45] = Gate_Descriptor((u64)&isr45, IST, INTGATE);
    k_idt.entries[46] = Gate_Descriptor((u64)&isr46, IST, INTGATE);
    k_idt.entries[47] = Gate_Descriptor((u64)&isr47, IST, INTGATE);

    k_idt.entries[PMOS_SYSCALL_INT] = Gate_Descriptor((u64)&isr202, IST, TRAPGATE);


    mask_PIC();

    IDT_descriptor desc = {sizeof(IDT) - 1, (uint64_t)&k_idt};
    loadIDT(&desc);
}

void init_kernel_stack()
{
    kernel_stack = (Stack*)palloc(sizeof(Stack)/4096);
    kernel_gdt.SSD_entries[0] = System_Segment_Descriptor((uint64_t) calloc(1,sizeof(TSS)), sizeof(TSS), 0x89, 0x02);
    kernel_gdt.SSD_entries[0].tss()->ist1 = (uint64_t)kernel_stack + sizeof(Stack);
    loadTSS(0x28);
}

void init_interrupts()
{
    init_IDT();
    init_kernel_stack();
}

extern "C" void interrupt_handler()
{
    Interrupt_Register_Frame *stack_frame = &current_task->regs; 
    u64 intno = stack_frame->intno;

    switch (intno) {
        case 0x0: 
            t_print("!!! Division by zero (DE)\n");
            break;
        case 0x4:
            t_print("!!! Overflow (OF)\n");
            break;
        case 0x6: 
            t_print("!!! Invalid op-code (UD) -> instruction %h\n", stack_frame->rip);
            halt();
            break;
        case 0x8: 
            t_print("!!! Double fault (DF) [ABORT]\n");
            break;
        case 0xE:
            pagefault_manager();
            break;
        case 0xD:
            t_print("!!! General Protection Fault (GP) error %h\n", stack_frame->err);
            print_registers(current_task);
            halt();
            break;
        case PMOS_SYSCALL_INT:
            //t_print("Syscall. Jumping to the handler\n");
            syscall_handler(current_task);
            break;;
        default:
            t_print("!!! Unknown interrupt %h. Not currently handled.\n", intno);
            break;
    }
}