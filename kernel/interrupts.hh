#pragma once
#include <stdint.h>
#include "types.hh"

struct PACKED Gate_Descriptor {
    uint64_t offset0        :16;
    uint16_t segment_sel    :16;
    uint8_t ist             :3;
    uint8_t reserved        :5;
    uint8_t gate_type       :4;
    uint8_t zero            :1;
    uint8_t dpl             :2;
    uint8_t present         :1;
    uint64_t offset1        :48;
    uint64_t reserved1      :32;

    void set_address(void* addr);
    void init(void* addr, uint16_t segment, uint8_t gate, uint8_t priv_level);
};

struct PACKED IDT {
    Gate_Descriptor entries[256];
};

struct PACKED IDT_descriptor {
    u16 size;
    u64 offset;
};

extern IDT k_idt;

void init_interrupts();