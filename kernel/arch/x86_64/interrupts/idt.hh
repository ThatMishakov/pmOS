#pragma once
#include <types.hh>
#include "gdt.hh"

extern "C" typedef void( *isr )(void);

constexpr u8 interrupt_gate_type = 0b1110;
constexpr u8 trap_gate_type      = 0b1111;

struct Gate_Descriptor {
    u16 offset_1            = 0;
    u16 segment_selector    = 0;
    u8  ist             :3  = 0;
    u8  reserved        :5  = 0;
    u8  gate_type       :4  = 0;
    u8  zero            :1  = 0;
    u8  dpl             :2  = 0;
    u8  present         :1  = 0;
    u16 offset_2            = 0;
    u32 offset_3            = 0;
    u32 reserved_1          = 0;

    Gate_Descriptor() = default;

    constexpr Gate_Descriptor(isr offset, u8 gate_type, u8 ist, u8 privilege_level, u16 segment_selector = R0_CODE_SEGMENT): 
        offset_1((u64)(offset)), segment_selector(segment_selector), ist(ist), reserved(0), gate_type(gate_type),
        zero(0), dpl(privilege_level), present(1), offset_2((u64)(offset) >> 16), offset_3((u64)(offset) >> 32), reserved_1(0)
        {}

} PACKED ALIGNED(8);

struct IDT {
    Gate_Descriptor entries[256];

    constexpr void register_isr(u16 intno, isr isr_routine, u8 gate_type, u8 ist, u8 allowed_privilege_level)
    {
        entries[intno] = {isr_routine, gate_type, ist, allowed_privilege_level};
    }

    constexpr void invalidate_isr(u16 intno)
    {
        entries[intno] = {};
    }
} PACKED ALIGNED(8);

extern IDT k_idt;

struct IDTR {
    u16 size;
    IDT *idt_offset;
} PACKED;

extern "C" void loadIDT(IDTR* IDT_desc);