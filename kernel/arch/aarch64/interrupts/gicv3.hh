#pragma once
#include <types.hh>

namespace kernel::arm::interrupts
{

void init_gicv3();

void gicv3_ppi_clear(u32 ppi);
void gicv3_ppi_enable(u32 ppi);

} // namespace kernel::arm::interrupts