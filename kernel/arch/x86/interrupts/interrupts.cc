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

#include "interrupts.hh"

#include "apic.hh"
#include <interrupts/gdt.hh>
#include <interrupts/idt.hh>

#include <cpus/ipi.hh>
#include <kernel/messaging.h>
#include <memory/malloc.hh>
#include <memory/palloc.hh>
#include <misc.hh>
#include <processes/syscalls.hh>
#include <sched/sched.hh>
#include <utils.hh>
#include <x86_asm.hh>
#include <utils.hh>

#ifdef __x86_64__
using namespace kernel::x86_64::interrupts;
#else
using namespace kernel::ia32::interrupts;
#endif
using namespace kernel::x86::interrupts;

void set_idt()
{
    IDTR desc = {sizeof(IDT) - 1, &k_idt};
    loadIDT(&desc);
}

void init_idt()
{
    lapic::enable_apic();
    set_idt();
}

void init_interrupts()
{
    init_idt();
    lapic::discover_apic_freq();
}

extern "C" void timer_interrupt()
{
    // apic_eoi was always at the end, but I think it needs to be at the
    // beginning so that the interrupts are not lost Not sure about it and don't
    // remember what it was doing exactly
    lapic::apic_eoi();

    kernel::sched::sched_periodic();
}

/*
extern "C" void interrupt_handler()
{
    const klib::shared_ptr<TaskDescriptor>& t = get_cpu_struct()->current_task;
    u64 intno = t->regs.intno;
    u64 err = t->regs.int_err;
    Interrupt_Stackframe* int_s = &t->regs.e;

    //t_print_bochs("Int %h error %h pid %h rip %h cs %h\n", intno, err, t->pid,
t->regs.e.rip, t->regs.e.cs);

    if (intno < 32)
    switch (intno) {
        case 0x0:
            t_print("!!! Division by zero (DE)\n");
            break;
        case 0x4:
            t_print("!!! Overflow (OF)\n");
            break;
        case 0x8:
            t_print_bochs("!!! Double fault (DF) [ABORT]\n");
            halt();
            break;
        default:
            t_print_bochs("!!! Unknown exception %h. Not currently handled.\n",
intno); halt(); break;
    }
    else if (intno < 48) {
        // Spurious PIC interrupt
    } else if (intno < 0xf0) {
        programmable_interrupt(intno);
    } else if (intno == 0xfb) {
        sched_periodic();
        // smart_eoi(intno);
    }
}
*/

void (*return_table[6])(void) = {
    ret_from_interrupt,
    ret_from_syscall,
    ret_from_sysenter,
    ret_repeat_syscall,
    return_from_kernel_thread,
    return_to_userspace_mask_interrupts,
};