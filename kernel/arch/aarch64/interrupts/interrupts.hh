#pragma once
#include <types.hh>

namespace kernel::arm::interrupts
{

void init_interrupt_controllers();

void ppi_clear(u32 ppi);
void ppi_enable(u32 ppi);

} // namespace kernel::arm::interrupts