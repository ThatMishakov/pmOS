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

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H 1

#define __DECLARE_CLOCK_T
#define __DECLARE_TIME_T
#include "__posix_types.h"

struct tms {
    clock_t tms_utime;  //< User CPU time.
    clock_t tms_stime;  //< System CPU time.
    clock_t tms_cutime; //< User CPU time of terminated child processes.
    clock_t tms_cstime; //< System CPU time of terminated child processes.
};

#ifdef __STDC_HOSTED__

    #ifdef __cplusplus
extern "C" {
    #endif

/**
 * @brief Get process times.
 *
 * The `times` function returns the current process times.
 *
 * @param buffer A pointer to a `tms` structure to be filled with the current process times.
 * @return       The `times` function returns the number of clock ticks that have elapsed
 *               since an arbitrary point in the past (for example, system startup); this
 *               point does not change within different invocations of the function.
 *               If an error occurs, the function returns `-1`.
 */
clock_t times(struct tms *buffer);

    #ifdef __cplusplus
} /* extern "C" */
    #endif

#endif // __STDC_HOSTED__

#endif // _SYS_TIMES_H