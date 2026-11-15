#pragma once
#include <types.hh>
#include <array>

namespace kernel::arm::interrupts
{

void init_interrupt_controllers();

void ppi_clear(u32 ppi);
void ppi_enable(u32 ppi);
void ppi_disable(u32 ppi);

void do_cpu_interrupts_init();

void handle_ppi_spi(unsigned interrupt);

// Starting at 16
extern std::array<void (*)(), 16> ppi_handlers;

} // namespace kernel::arm::interrupts