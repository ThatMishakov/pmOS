#pragma once
#include <types.hh>

/// IPI vector calling reschedule() function upon recieving the interrupt
constexpr u8 ipi_reschedule_int_vec = 0xfa;

/// ISR for ipi_reschedule_int_vec calling reschedule()
extern "C" void ipi_reschedule_isr();

/// Interrupt vector for invalidate_tlb routine
constexpr u8 ipi_invalidate_tlb_int_vec = 0xfb;
extern "C" void ipi_invalidate_tlb_isr();
extern "C" void ipi_invalidate_tlb_routine();