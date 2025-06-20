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

#include <cpus/sse.hh>
#include <processes/tasks.hh>
#include <interrupts/gdt.hh>
#include <x86_asm.hh>

using namespace kernel::proc;

void save_segments(TaskDescriptor *task);
void restore_segments(TaskDescriptor *task);

void TaskDescriptor::before_task_switch()
{
    save_segments(this);

    if (x86::sse::sse_is_valid()) {
        // t_print_bochs("Saving SSE registers for PID %h\n",
        // c->current_task->pid);
        sse_data.save_sse();
        x86::sse::invalidate_sse();
        holds_sse_data = true;
    }
}

void TaskDescriptor::after_task_switch() {
    if (holds_sse_data) {
        // t_print_bochs("Restoring SSE registers for PID %h\n",
        // c->current_task->pid);
        x86::sse::validate_sse();
        sse_data.restore_sse();
    }
    restore_segments(this);
}

bool TaskDescriptor::is_kernel_task() const
{
    return regs.get_cs() == R0_CODE_SEGMENT;
}