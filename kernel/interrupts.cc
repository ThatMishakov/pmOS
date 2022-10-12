#include "interrupts.hh"
#include "asm.hh"

IDT k_idt = {0};

void init_interrupts()
{
    // TODO

    IDT_descriptor desc = {sizeof(IDT) - 1, (uint64_t)&k_idt};
    loadIDT(&desc);
}