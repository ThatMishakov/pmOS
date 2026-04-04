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

#ifndef _SCHED_H
#define _SCHED_H 1

#define __DECLARE_PID_T
#define __DECLARE_TIME_T
#include "__posix_types.h"

#include <time.h>

struct sched_param {
    int sched_priority;                   //< Process execution scheduling priority.
    int sched_ss_low_priority;            //< Low scheduling priority for sporadic server.
    struct timespec sched_ss_repl_period; //< Replenishment period for sporadic server.
    struct timespec sched_ss_init_budget; //< Initial budget for sporadic server.
    int sched_ss_max_repl;                //< Maximum pending replenishments for sporadic server.
};

/// Scheduling policies constants
enum {
    SCHED_FIFO     = 0, //< First in, first out scheduling policy.
    SCHED_RR       = 1, //< Round robin scheduling policy.
    SCHED_SPORADIC = 2, //< Sporadic server scheduling policy.
    SCHED_OTHER    = 3, //< Another scheduling policy.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int sched_get_priority_max(int);
int sched_get_priority_min(int);

int sched_getparam(pid_t, struct sched_param *);
int sched_getscheduler(pid_t);

int sched_rr_get_interval(pid_t, struct timespec *);

int sched_setparam(pid_t, const struct sched_param *);
int sched_setscheduler(pid_t, int, const struct sched_param *);

int sched_yield(void);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SCHED_H