/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <lib/memory.hh>
#include <memory/palloc.hh>

void init_interrupts();

void set_idt();

extern "C" void interrupt_handler();

// Different ways of returning from kernel. The right function is chosen upon
// entering the kernel and is subsequently called upon returning from kernel
// back to the userspace. This is neede because stacks are not switched upon
// task switches. These functions are called at the ends on the kernel entry
// points, defined in assemnly
extern "C" void
    ret_from_interrupt(void) NORETURN; ///< Assembly function used for returning from an interrupt
extern "C" void ret_from_syscall(
    void) NORETURN; ///< Assembly function used for returning from SYSCALL instruction
extern "C" void ret_from_sysenter(void) NORETURN; ///< Assembly function used for returning
                                                  ///< from SYSENTER instruction
extern "C" void ret_repeat_syscall(
    void) NORETURN; ///< Assembly funtion used for returning from a repeated syscall.
                    ///< This function does not actually return to the userspace, but
                    ///< restores the previous context and reexecutes the last syscall
                    ///< that the process was issuing before blocking.

extern void (*return_table[4])(void);

extern "C" void lvt0_int_isr();
extern "C" void lvt1_int_isr();
extern "C" void apic_timer_isr();
extern "C" void apic_spurious_isr();
extern "C" void apic_dummy_isr();

extern "C" void dummy_isr();
