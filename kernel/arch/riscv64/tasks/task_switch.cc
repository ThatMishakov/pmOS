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

#include <cpus/floating_point.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>

void TaskDescriptor::before_task_switch()
{
    u32 fp_state = get_fp_state();
    if (fp_state == FloatingPointState::Disabled) {
        return;
    }

    auto c = get_cpu_struct();

    switch (fp_state) {
    case FloatingPointState::Disabled:
        break;
    case FloatingPointState::Initial:
        this->fp_state        = FloatingPointState::Initial;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task       = task_id;
        c->last_fp_state      = FloatingPointState::Initial;
        break;
    case FloatingPointState::Clean:
        this->fp_state        = FloatingPointState::Clean;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task       = task_id;
        c->last_fp_state      = FloatingPointState::Clean;
        break;
    case FloatingPointState::Dirty:
        if (!this->fp_registers)
            this->fp_registers =
                klib::unique_ptr<u64[]>(new u64[fp_register_size(max_supported_fp_level) * 2]);

        asm("csrr %0, fcsr" : "=r"(this->fcsr));
        save_fp_registers(fp_registers.get());
        this->fp_state        = FloatingPointState::Clean;
        this->last_fp_hart_id = c->hart_id;
        c->last_fp_task       = this->task_id;
        c->last_fp_state      = FloatingPointState::Clean;
        break;
    }

    set_fp_state(FloatingPointState::Disabled);
}

void TaskDescriptor::after_task_switch()
{
    auto c = get_cpu_struct();
    if ((fp_state == FloatingPointState::Disabled or fp_state == FloatingPointState::Initial) and
        c->last_fp_state == FloatingPointState::Initial) {
        set_fp_state(FloatingPointState::Initial);
        return;
    }

    if (c->last_fp_task == task_id and last_fp_hart_id == c->hart_id) {
        set_fp_state(FloatingPointState::Clean);
        return;
    }
}