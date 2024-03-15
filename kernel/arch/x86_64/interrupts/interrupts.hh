#pragma once
#include "types.hh"
#include "gdt.hh"
#include <memory/palloc.hh>
#include <lib/memory.hh>

void init_interrupts();

void set_idt();

extern "C" void interrupt_handler();

// Different ways of returning from kernel. The right function is chosen upon entering the kernel and
// is subsequently called upon returning from kernel back to the userspace. This is neede because stacks are
// not switched upon task switches. These functions are called at the ends on the kernel entry points, defined
// in assemnly
extern "C" void ret_from_interrupt(void) NORETURN; ///< Assembly function used for returning from an interrupt
extern "C" void ret_from_syscall(void) NORETURN; ///< Assembly function used for returning from SYSCALL instruction
extern "C" void ret_from_sysenter(void) NORETURN; ///< Assembly function used for returning from SYSENTER instruction
extern "C" void ret_repeat_syscall(void) NORETURN; ///< Assembly funtion used for returning from a repeated syscall.
                                                   ///< This function does not actually return to the userspace, but restores the previous context
                                                   ///< and reexecutes the last syscall that the process was issuing before blocking.

extern void (*return_table[4])(void);

extern "C" void lvt0_int_isr();
extern "C" void lvt1_int_isr();
extern "C" void apic_timer_isr();

extern "C" void dummy_isr();
