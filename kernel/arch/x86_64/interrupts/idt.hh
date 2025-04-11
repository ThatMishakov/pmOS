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
#include "gdt.hh"

#include <types.hh>

namespace kernel::x86_64::interrupts
{

extern "C" typedef void (*isr)(void);

constexpr u8 interrupt_gate_type = 0b1110;
constexpr u8 trap_gate_type      = 0b1111;

struct Gate_Descriptor {
    u16 offset_1         = 0;
    u16 segment_selector = 0;
    u8 ist : 3           = 0;
    u8 reserved : 5      = 0;
    u8 gate_type : 4     = 0;
    u8 zero : 1          = 0;
    u8 dpl : 2           = 0;
    u8 present : 1       = 0;
    u16 offset_2         = 0;
    u32 offset_3         = 0;
    u32 reserved_1       = 0;

    Gate_Descriptor() = default;

    Gate_Descriptor(isr offset, u8 gate_type, u8 ist, u8 privilege_level,
                    u16 segment_selector = R0_CODE_SEGMENT)
        : offset_1((u64)(offset)), segment_selector(segment_selector), ist(ist), reserved(0),
          gate_type(gate_type), zero(0), dpl(privilege_level), present(1),
          offset_2((u64)(offset) >> 16), offset_3((u64)(offset) >> 32), reserved_1(0)
    {
    }

} PACKED ALIGNED(8);

struct IDT {
    Gate_Descriptor entries[256];

    void register_isr(u16 intno, isr isr_routine, u8 gate_type, u8 ist, u8 allowed_privilege_level)
    {
        entries[intno] = {isr_routine, gate_type, ist, allowed_privilege_level};
    }

    constexpr void invalidate_isr(u16 intno) { entries[intno] = {}; }
} PACKED ALIGNED(8);

extern IDT k_idt;

struct IDTR {
    u16 size;
    IDT *idt_offset;
} PACKED;

extern "C" void loadIDT(IDTR *IDT_desc);

} // namespace kernel::x86_64::interrupts