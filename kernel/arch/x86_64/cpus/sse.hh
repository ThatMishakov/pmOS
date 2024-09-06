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

/**
 * @brief Structure holding SSE data for x86 CPUs
 *
 * This structure is used to preserve the state of the registers used by SSE
 * extensions. Although the kernel should not use those, these are needed to be
 * saved upon task switches. Currently, the kernel allocates this structure for
 * every process (and threads are a special case of processes in pmOS) and
 * lazilly stores it upon task switch (checking if they have been modified by
 * the process during its execution) and restored when needed.
 *
 * In order to know if the SSE registers are valid, a CR0.TS bit is checked by
 * the CPU. If it is set and the task tries to use the SIMD instruction, a
 * Device Not Available (#NM) exception is raised. The kernel then restores the
 * SSE data, clears the bit and lets the process continue its execution.
 * Similarly, upon the task switch, this bit is checked and if it is not set,
 * the SSE registers are needed to be saved. \see enable_sse() sse_is_valid()
 * invalidate_sse() validate_sse() sse_exception_manager() \todo A more clever
 * algorithm might be used to avoid saving and restoring this data if it was not
 * modified by other processes between the task switches.
 */
struct SSE_Data {
    u8 data[0];
    u16 fcw        = 0x37F;
    u16 fsw        = 0x0;
    u8 ftw         = 0x0;
    u8 reserved    = 0x0;
    u16 fop        = 0x0;
    u32 fpu_ip     = 0x0;
    u16 fpu_cs     = 0x0;
    u16 reserved1  = 0x0;
    u32 fpu_dp     = 0x0;
    u16 fpu_ds     = 0x0;
    u16 reserved2  = 0x0;
    u32 mxcsr      = 0x1F80;
    u32 mxcsr_mask = 0xFFFF;

    u8 mm0[16] = {0};
    u8 mm1[16] = {0};
    u8 mm2[16] = {0};
    u8 mm3[16] = {0};
    u8 mm4[16] = {0};
    u8 mm5[16] = {0};
    u8 mm6[16] = {0};
    u8 mm7[16] = {0};

    u8 xmm0[16]  = {0};
    u8 xmm1[16]  = {0};
    u8 xmm2[16]  = {0};
    u8 xmm3[16]  = {0};
    u8 xmm4[16]  = {0};
    u8 xmm5[16]  = {0};
    u8 xmm6[16]  = {0};
    u8 xmm7[16]  = {0};
    u8 xmm8[16]  = {0};
    u8 xmm9[16]  = {0};
    u8 xmm10[16] = {0};
    u8 xmm11[16] = {0};
    u8 xmm12[16] = {0};
    u8 xmm13[16] = {0};
    u8 xmm14[16] = {0};
    u8 xmm15[16] = {0};

    u8 reserved3[96] = {0};

    inline void save_sse() { asm volatile(" fxsave %0 " ::"m"(data)); }
    inline void restore_sse() { asm volatile(" fxrstor %0 " ::"m"(data)); }
} ALIGNED(16);

/**
 * \brief Sets the appropriate bits in the registers to allow the processes to
 * use the SSE instructions
 *
 */
void enable_sse();

/// Checks if SSE state is valid (CR0.TS == 0)
bool sse_is_valid();

/// \brief Sets CR0.TS to signal that the SSE registers are not valid (not
/// usable by the process) \see validate_sse() \see SSE_Data
void invalidate_sse();

/// \brief Clears the CR0.TS bit, indicating that the SSE registers are in a
/// valid state \see invalidate_sse() \see SSE_Data
void validate_sse();