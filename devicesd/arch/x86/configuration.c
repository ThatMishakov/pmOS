/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <configuration.h>
#include <interrupts/interrupts.h>
#include <ioapic/ioapic.h>
#include <pmos/interrupts.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdbool.h>

uint8_t get_ioapic_int(uint32_t intno, uint64_t dest_pid, uint64_t chan)
{
    uint8_t cpu_int_vector = get_free_interrupt();

    // result_t kern_result = set_port_kernel(KERNEL_MSG_INT_START+cpu_int_vector, dest_pid, chan);
    result_t kern_result = set_interrupt(chan, cpu_int_vector, 0);
    if (kern_result != SUCCESS)
        return 0;

    bool result = program_ioapic(cpu_int_vector, intno);
    return result ? cpu_int_vector : 0;
}

uint8_t configure_interrupts_for(IPC_Reg_Int *m)
{
    if (m->flags & IPC_Reg_Int_FLAG_EXT_INTS) {
        return get_ioapic_int(m->intno, m->dest_task, m->dest_chan);
    }

    // Not supported yet
    return 0;
}