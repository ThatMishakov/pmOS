#include "interrupts.hh"
#include "utils.hh"
#include "gdt.hh"

constexpr Gate_Descriptor::Gate_Descriptor() 
    : Gate_Descriptor(0, 0, 0)
{}

constexpr Gate_Descriptor::Gate_Descriptor(uint64_t offset, uint8_t ist, uint8_t type_attr)
    : offset0(offset & 0xffff),
    segment_sel(KERNEL_CODE_SELECTOR),
    ist(ist),
    attributes(type_attr),
    offset1((offset >> 16) & 0xffff),
    offset2((offset >> 32) & 0xffffffff),
    reserved1(0)
{}


IDT k_idt = {};

void init_interrupts()
{
     k_idt.entries[0] = Gate_Descriptor((u64)&isr0, 0, INTGATE);
     k_idt.entries[1] = Gate_Descriptor((u64)&isr1, 0, TRAPGATE);
     k_idt.entries[2] = Gate_Descriptor((u64)&isr2, 0, INTGATE);
     k_idt.entries[3] = Gate_Descriptor((u64)&isr3, 0, TRAPGATE);
     k_idt.entries[4] = Gate_Descriptor((u64)&isr4, 0, TRAPGATE);
     k_idt.entries[5] = Gate_Descriptor((u64)&isr5, 0, INTGATE);
     k_idt.entries[6] = Gate_Descriptor((u64)&isr6, 0, INTGATE);
     k_idt.entries[7] = Gate_Descriptor((u64)&isr7, 0, INTGATE);
     k_idt.entries[8] = Gate_Descriptor((u64)&isr8, 0, INTGATE);
     k_idt.entries[9] = Gate_Descriptor((u64)&isr9, 0, INTGATE);
     k_idt.entries[10] = Gate_Descriptor((u64)&isr10, 0, INTGATE);
     k_idt.entries[11] = Gate_Descriptor((u64)&isr11, 0, INTGATE);
     k_idt.entries[12] = Gate_Descriptor((u64)&isr12, 0, INTGATE);
     k_idt.entries[13] = Gate_Descriptor((u64)&isr13, 0, INTGATE);
     k_idt.entries[14] = Gate_Descriptor((u64)&isr14, 0, INTGATE);
     k_idt.entries[15] = Gate_Descriptor((u64)&isr15, 0, INTGATE);
     k_idt.entries[16] = Gate_Descriptor((u64)&isr16, 0, INTGATE);
     k_idt.entries[17] = Gate_Descriptor((u64)&isr17, 0, INTGATE);
     k_idt.entries[18] = Gate_Descriptor((u64)&isr18, 0, INTGATE);
     k_idt.entries[19] = Gate_Descriptor((u64)&isr19, 0, INTGATE);
     k_idt.entries[20] = Gate_Descriptor((u64)&isr20, 0, INTGATE);
     k_idt.entries[21] = Gate_Descriptor((u64)&isr21, 0, INTGATE);
     k_idt.entries[22] = Gate_Descriptor((u64)&isr22, 0, INTGATE);
     k_idt.entries[23] = Gate_Descriptor((u64)&isr23, 0, INTGATE);
     k_idt.entries[24] = Gate_Descriptor((u64)&isr24, 0, INTGATE);
     k_idt.entries[25] = Gate_Descriptor((u64)&isr25, 0, INTGATE);
     k_idt.entries[26] = Gate_Descriptor((u64)&isr26, 0, INTGATE);
     k_idt.entries[27] = Gate_Descriptor((u64)&isr27, 0, INTGATE);
     k_idt.entries[28] = Gate_Descriptor((u64)&isr28, 0, INTGATE);
     k_idt.entries[29] = Gate_Descriptor((u64)&isr29, 0, INTGATE);
     k_idt.entries[30] = Gate_Descriptor((u64)&isr30, 0, INTGATE);
     k_idt.entries[31] = Gate_Descriptor((u64)&isr31, 0, INTGATE);


    IDT_descriptor desc = {sizeof(IDT) - 1, (uint64_t)&k_idt};
    t_print("IDT_Desc: size %h addr %h\n", desc.size, desc.offset);
    loadIDT(&desc);
}

extern "C" u64 interrupt_handler(u64 rsp)
{
    Interrupt_Stack_Frame *stack_frame = reinterpret_cast<Interrupt_Stack_Frame *>(rsp); 
    u64 intno = stack_frame->intno;

    t_print("Recieved interrupt: 0x%h -> ", stack_frame->intno);
    switch (intno) {
        case 0x0: 
            t_print("Division by zero (DE)");
            break;
        case 0x4:
            t_print("Overflow (OF)");
            break;
        case 0x6: 
            t_print("Invalid op-code (UD)");
            break;
        case 0x8: 
            t_print("Double fault (DF) [ABORT]");
            break;
        case 0xE:
            t_print("Page fault (PE) - Error code: 0x%h", stack_frame->err);
            break;
        case 0xD:
            t_print("General Protection Fault (GP)");
            break;
        default:
            t_print("Not currently handled.");
            break;
    }
    t_print("\n");
    return rsp;
}
