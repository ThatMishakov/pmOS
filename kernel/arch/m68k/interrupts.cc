#include <types.hh>
#include <interrupts/interrupt_handler.hh>

namespace kernel::interrupts
{

void interrupt_enable(InterruptHandler *handler)
{
    panic("not implemented");
}
void interrupt_disable(InterruptHandler *handler)
{
    panic("not implemented");
}
void interrupt_complete(InterruptHandler *handler)
{
    panic("not implemented");
}

ReturnStr<InterruptHandler *> allocate_or_get_handler(u32 gsi, bool edge_triggered, bool active_low)
{
    panic("not implemented");
}

} // namespace kernel::interrupts