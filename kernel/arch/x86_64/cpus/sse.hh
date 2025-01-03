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
#include <lib/memory.hh>

enum class SSECtxStyle {
    FXSAVE,
    XSAVE,
    XSAVEOPT,
};

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
    klib::unique_ptr<u8> data = nullptr;
    bool holds_state = false;

    /// \brief Initializes the SSE data structure
    ///
    /// @return 0 on success, error code otherwise (e.g. out of memory)
    kresult_t init_on_thread_start();

    void save_sse();
    void restore_sse();
};

/**
 * \brief Sets the appropriate bits in the registers to allow the processes to
 * use the SSE instructions
 *
 */
void enable_sse();

/**
 * \brief Detects the SSE instructions supported by the CPU (and what to use to save/restore the state)
 * \see SSECtxStyle
 */
void detect_sse();

/// Checks if SSE state is valid (CR0.TS == 0)
bool sse_is_valid();

/// \brief Sets CR0.TS to signal that the SSE registers are not valid (not
/// usable by the process) \see validate_sse() \see SSE_Data
void invalidate_sse();

/// \brief Clears the CR0.TS bit, indicating that the SSE registers are in a
/// valid state \see invalidate_sse() \see SSE_Data
void validate_sse();