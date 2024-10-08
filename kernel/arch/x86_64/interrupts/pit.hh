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

#define PIT_CH0_DATA 0x40
#define PIT_CH1_DATA 0x41
#define PIT_CH2_DATA 0x42
#define PIT_MODE_REG 0x43

struct PACKED PIT_Cmd {
    u8 bcd_bin : 1     = 0;
    u8 op_mode : 3     = 0;
    u8 access_mode : 2 = 0;
    u8 channel : 2     = 0;
};

struct PACKED PIT_RB_Cmd {
    u8 reserved : 1     = 0;
    u8 rb_tmr0 : 1      = 0;
    u8 rb_tmr1 : 1      = 0;
    u8 rb_tmr2 : 1      = 0;
    u8 latch_status : 1 = 0;
    u8 latch_count : 1  = 0;
    u8 rb_cmd : 2       = 3;
};

struct PACKED RB_Status {
    u8 bcd_bin : 1      = 0;
    u8 op_mode : 3      = 0;
    u8 access_mode : 2  = 0;
    u8 null_count_f : 1 = 0;
    u8 out_p_state : 1  = 0;

    constexpr RB_Status(u8 i)
        : bcd_bin(i & 0x01), op_mode((i >> 1) & 0x07), access_mode((i >> 4) & 0x03),
          null_count_f((i >> 6) & 0x01), out_p_state((i >> 7) & 0x01) {};
};

u16 read_pit_count(u8 chan = 0);
void set_pit_count(u16 count, u8 chan = 0);

void pit_sleep_100us(u16 time);