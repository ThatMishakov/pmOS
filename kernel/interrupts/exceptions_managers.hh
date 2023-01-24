#pragma once
#include "interrupts.hh"

void pagefault_manager(u64 err, Interrupt_Stackframe* int_s);

void sse_exception_manager();