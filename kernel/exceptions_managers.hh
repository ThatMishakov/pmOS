#pragma once
#include "interrupts.hh"

void pagefault_manager(uint64_t err, Interrupt_Stackframe* int_s);