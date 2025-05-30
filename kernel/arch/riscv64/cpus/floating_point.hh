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
#include <types.hh>

namespace kernel::proc
{
class TaskDescriptor;
}

namespace kernel::riscv64::fp
{

enum class FloatingPointSize {
    None            = 0,
    SinglePrecision = 1,
    DoublePrecision = 2,
    QuadPrecision   = 3,
};

/// Maximum supported floating point size by the system
/// Individual harts might theoretically support different sizes
// TODO: Assume heterogenous system for now
inline FloatingPointSize max_supported_fp_level = FloatingPointSize::None;

inline bool fp_is_supported() { return max_supported_fp_level != FloatingPointSize::None; }

constexpr int fp_register_size(FloatingPointSize size)
{
    switch (size) {
    case FloatingPointSize::None:
        return 0;
    case FloatingPointSize::SinglePrecision:
        return 32;
    case FloatingPointSize::DoublePrecision:
        return 64;
    case FloatingPointSize::QuadPrecision:
        return 128;
    }
    return 0;
}

enum FloatingPointState {
    Disabled = 0,
    Initial  = 1,
    Clean    = 2,
    Dirty    = 3,
};

// Sets sstatus FS field to the given value
inline void set_fp_state(FloatingPointState state)
{
    u64 sstatus = 0;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(0b11 << 13);
    sstatus |= (static_cast<u64>(state) << 13);
    asm volatile("csrw sstatus, %0" : : "r"(sstatus));
}

// Gets the current state of the floating point unit
inline FloatingPointState get_fp_state()
{
    u64 sstatus = 0;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    return static_cast<FloatingPointState>((sstatus >> 13) & 0b11);
}

// Restores the floating point state from the given task
void restore_fp_state(kernel::proc::TaskDescriptor *task);

// Saves the floating point registers to the given pointer
void save_fp_registers(u64 *fp_regs);

} // namespace kernel::riscv64::fp