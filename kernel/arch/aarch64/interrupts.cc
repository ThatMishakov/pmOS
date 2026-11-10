#include <interrupts/interrupt_handler.hh>

using namespace kernel;

void interrupts::interrupt_complete(interrupts::InterruptHandler*)
{
    panic("interrupt_complete() not implemented");
}

void interrupts::interrupt_disable(interrupts::InterruptHandler *)
{
    panic("interrupt_disable() not implemented");
}

void interrupts::interrupt_enable(interrupts::InterruptHandler *)
{
    panic("interrupt_enable() not implemented");
}

ReturnStr<interrupts::InterruptHandler *> interrupts::allocate_or_get_handler(u32 gsi, bool edge_triggered, bool)
{
    panic("allocate_or_get_handler() not implemented");
}
