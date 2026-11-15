#pragma once
#include <types.hh>
#include <interrupts/interrupt_handler.hh>

namespace kernel::arm::interrupts
{

void init_gicv3();

void gicv3_ppi_clear(u32 ppi);
void gicv3_ppi_enable(u32 ppi);
void gicv3_ppi_disable(u32 ppi);

ReturnStr<kernel::interrupts::InterruptHandler *> gicv3_get_handler(u32 gsi, bool edge_triggered, bool active_low);

void gicv3_enable(kernel::interrupts::InterruptHandler *handler);
void gicv3_disable(kernel::interrupts::InterruptHandler *handler);
void gicv3_interrupt_complete(kernel::interrupts::InterruptHandler *handler);

void gicv3_do_cpu_interrupts_init();
void gicv3_handle_irq();
void gicv3_handle_fiq();

} // namespace kernel::arm::interrupts