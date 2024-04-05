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

#ifndef TIMERS_H
#define TIMERS_H
#include <stdint.h>
#include <stdbool.h>
#include <pmos/ipc.h>

extern uint64_t ticks_ms;

extern enum Timer_Mode {
    TIMER_UNKNOWN,
    TIMER_HPET,
    TIMER_PIC
} timer_mode;

struct timer_info {
};

typedef struct timer_entry {
    uint64_t id;    
    uint64_t expires_at_ticks;

    uint64_t pid;
    uint64_t reply_chan;

    uint64_t extra0;
    uint64_t extra1;
    uint64_t extra2;
} timer_entry;

void init_timers();

uint64_t create_timer_ms(uint64_t ms);
void timer_tick();

// Returns 0 on success
int start_timer(IPC_Start_Timer* t, uint64_t pid);

uint64_t timer_calculate_next_ticks(uint64_t ms);
void update_ticks();
void start_oneshot_ticks(uint64_t ticks);
void notify_task(timer_entry* e);

void program_periodic(unsigned milliseconds);

#endif