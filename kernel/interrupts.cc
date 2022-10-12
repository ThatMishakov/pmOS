#include "interrupts.hh"
#include "utils.hh"

IDT k_idt = {};

void init_interrupts()
{
    // TODO

    IDT_descriptor desc = {sizeof(IDT) - 1, (uint64_t)&k_idt};
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
