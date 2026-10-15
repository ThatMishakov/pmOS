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


struct InterruptFrame {
    u32 d[8];
    u32 a[7];
    u16 int_frame[];
};

extern "C" void kernel_exception_handler(InterruptFrame *frame)
{
    panic("Got a kernel interrupt");
}

extern "C" void user_exception_handler()
{
    panic("Got a user interrupt");
}