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

#include "ipi.hh"

#include <interrupts/apic.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <x86_asm.hh>

void ipi_invalidate_tlb_routine()
{
    auto c = get_cpu_struct();

    auto val = __atomic_load_n(&c->ipi_mask, __ATOMIC_CONSUME);
    if (val & CPU_Info::ipi_synchronous_mask) {
        __atomic_and_fetch(&c->ipi_mask, ~CPU_Info::IPI_TLB_SHOOTDOWN, __ATOMIC_SEQ_CST);
        c->current_task->page_table->trigger_shootdown(c);
    }
    apic_eoi();
}

void signal_tlb_shootdown() { send_ipi_fixed_others(ipi_invalidate_tlb_int_vec); }

void reschedule_isr()
{
    reschedule();
    apic_eoi();
}

void CPU_Info::ipi_reschedule() { 
    send_ipi_fixed(ipi_reschedule_int_vec, lapic_id);
}
void CPU_Info::ipi_tlb_shootdown()
{
    __atomic_or_fetch(&ipi_mask, IPI_TLB_SHOOTDOWN, __ATOMIC_ACQUIRE);
    send_ipi_fixed(ipi_invalidate_tlb_int_vec, lapic_id);
}