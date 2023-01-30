#pragma once
#include <types.hh>

constexpr u8 ipi_reschedule_int_vec = 0xfa;
extern "C" void ipi_reschedule_isr();

constexpr u8 ipi_invalidate_tlb_int_vec = 0xfb;
extern "C" void ipi_invalidate_tlb_isr();
extern "C" void ipi_invalidate_tlb_routine();