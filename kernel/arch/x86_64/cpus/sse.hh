#pragma once
#include <types.hh>

/**
 * @brief Structure holding SSE data for x86 CPUs
 * 
 * This structure is used to preserve the state of the registers used by SSE extensions.
 * Although the kernel should not use those, these are needed to be saved upon task switches.
 * Currently, the kernel allocates this structure for every process (and threads are a special case
 * of processes in pmOS) and lazilly stores it upon task switch (checking if they have been modified
 * by the process during its execution) and restored when needed.
 * 
 * In order to know if the SSE registers are valid, a CR0.TS bit is checked by the CPU. If it is set and
 * the task tries to use the SIMD instruction, a Device Not Available (#NM) exception is raised. The kernel
 * then restores the SSE data, clears the bit and lets the process continue its execution. Similarly, upon
 * the task switch, this bit is checked and if it is not set, the SSE registers are needed to be saved.
 * \see enable_sse() sse_is_valid() invalidate_sse() validate_sse() sse_exception_manager()
 * \todo A more clever algorithm might be used to avoid saving and restoring this data if it was not modified
 *       by other processes between the task switches.
 */
struct SSE_Data {
    char fxsave_region[512] ALIGNED(16) = {0};

    inline void save_sse()
    {
        asm volatile(" fxsave %0 "::"m"(fxsave_region));
    }


    inline void restore_sse()
    {
        asm volatile(" fxrstor %0 "::"m"(fxsave_region));
    }
};

/**
 * \brief Sets the appropriate bits in the registers to allow the processes to use the SSE instructions
 * 
 */
void enable_sse();

/// Checks if SSE state is valid (CR0.TS == 0)
bool sse_is_valid();

/// \brief Sets CR0.TS to signal that the SSE registers are not valid (not usable by the process)
/// \see validate_sse()
/// \see SSE_Data
void invalidate_sse();

/// \brief Clears the CR0.TS bit, indicating that the SSE registers are in a valid state
/// \see invalidate_sse()
/// \see SSE_Data
void validate_sse();