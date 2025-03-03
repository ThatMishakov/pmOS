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

inline void halt()
{
    while (1) {
        asm("hlt");
    }
}

static inline void outb(u16 port, u8 data) { asm volatile("outb %0, %1" ::"a"(data), "Nd"(port)); }

static inline void io_wait() { outb(0x80, 0); }

static inline u8 inb(u16 port)
{
    u8 ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

struct CPUIDoutput {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

static inline CPUIDoutput cpuid(u32 p)
{
    CPUIDoutput out;
    asm volatile("cpuid" : "=a"(out.eax), "=b"(out.ebx), "=c"(out.ecx), "=d"(out.edx) : "a"(p));
    return out;
}

static inline CPUIDoutput cpuid2(u32 p, u32 q)
{
    CPUIDoutput out;
    asm volatile("cpuid"
                 : "=a"(out.eax), "=b"(out.ebx), "=c"(out.ecx), "=d"(out.edx)
                 : "a"(p), "c"(q));
    return out;
}

static inline void set_xcr(u32 reg, u64 val)
{
    ulong eax = val;
    ulong ecx = reg;
    ulong edx = val >> 32;

    asm volatile("xsetbv" ::"a"(eax), "c"(ecx), "d"(edx));
}

inline void x86_pause()
{
    asm volatile ("pause");
}