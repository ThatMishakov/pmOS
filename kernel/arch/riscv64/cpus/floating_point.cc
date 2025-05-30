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

#include "floating_point.hh"

#include <assert.h>
#include <processes/tasks.hh>
#include <sched/sched.hh>

using namespace kernel::riscv64::fp;
using namespace kernel::proc;

extern "C" void fp_write_regs_initial();
extern "C" void fp_load_regs_single(const u64 *fp_regs);
extern "C" void fp_load_regs_double(const u64 *fp_regs);
extern "C" void fp_store_regs_single(u64 *fp_regs);
extern "C" void fp_store_regs_double(u64 *fp_regs);

namespace kernel::riscv64::fp
{

void set_fp_state_initial()
{
    set_fp_state(FloatingPointState::Initial);

    static const u64 zero = 0;
    asm("csrw fcsr, %0" : : "r"(zero));

    fp_write_regs_initial();

    set_fp_state(FloatingPointState::Initial);
}

void restore_fp_state_single(TaskDescriptor *task)
{
    assert(task != nullptr and task->fp_registers);

    set_fp_state(FloatingPointState::Dirty);

    asm("csrw fcsr, %0" : : "r"(task->fcsr));
    const u64 *fp_regs = task->fp_registers.get();
    fp_load_regs_single(fp_regs);

    set_fp_state(FloatingPointState::Clean);
}

void restore_fp_state_double(TaskDescriptor *task)
{
    assert(task != nullptr and task->fp_registers);

    set_fp_state(FloatingPointState::Dirty);

    asm("csrw fcsr, %0" : : "r"(task->fcsr));
    const u64 *fp_regs = task->fp_registers.get();
    fp_load_regs_double(fp_regs);

    set_fp_state(FloatingPointState::Clean);
}

void restore_fp_state(TaskDescriptor *task)
{
    assert(task != nullptr and
           not(task->fp_state == FloatingPointState::Clean and !task->fp_registers));

    if (task->fp_state == FloatingPointState::Disabled or
        task->fp_state == FloatingPointState::Initial) {
        set_fp_state_initial();
        return;
    }

    switch (max_supported_fp_level) {
    case FloatingPointSize::SinglePrecision:
        restore_fp_state_single(task);
        break;
    case FloatingPointSize::DoublePrecision:
        restore_fp_state_double(task);
        break;
    default:
        assert(false);
    }
}

void save_fp_registers(u64 *fp_regs)
{
    assert(fp_regs);

    switch (max_supported_fp_level) {
    case FloatingPointSize::SinglePrecision:
        fp_store_regs_single(fp_regs);
        break;
    case FloatingPointSize::DoublePrecision:
        fp_store_regs_double(fp_regs);
        break;
    default:
        assert(false);
    }
}

} // namespace kernel::riscv64::fp

kresult_t TaskDescriptor::init_fp_state()
{
    if (!is_system) {
        fp_registers =
            klib::unique_ptr<u64[]>(new u64[fp_register_size(max_supported_fp_level) * 2]);
        if (!fp_registers)
            return -ENOMEM;
    }

    return 0;
}