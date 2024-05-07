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

#include "pit.hh"

#include <utils.hh>
#include <x86_utils.hh>

u16 read_pit_count(u8 chan)
{
    PIT_Cmd c = {0, 0, 0, chan};

    outb(PIT_MODE_REG, *(u8 *)&c);

    u16 count;
    count = inb(PIT_CH0_DATA + chan);
    count |= inb(PIT_CH0_DATA + chan) << 8;
    return count;
}

void set_pit_count(u16 count, u8 chan)
{
    outb(PIT_CH0_DATA + chan, count & 0xff);
    outb(PIT_CH0_DATA + chan, count >> 8);
}

void pit_sleep_100us(u16 time)
{
    outb(PIT_MODE_REG, 0b10'11'001'0);
    set_pit_count(119 * time, 2);

    // Start timer 2
    u8 p = inb(0x61);
    p    = (p & 0xfe);
    outb(0x61, p);     // Gate LOW
    outb(0x61, p | 1); // Gate HIGH

    while (not(inb(0x61) & 0x20))
        ;
}
