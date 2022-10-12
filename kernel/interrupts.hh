#pragma once
#include <stdint.h>
#include "types.hh"

#define INTGATE 0x8e
#define TRAPGATE 0xee

#define IDT_USER 0b01100000


struct PACKED Gate_Descriptor {
    uint64_t offset0        :16;
    uint16_t segment_sel    :16;
    uint8_t ist             :3;
    uint8_t reserved        :5;
    uint8_t attributes      :4;
    uint64_t offset1        :48;
    uint64_t reserved1      :32;

    void set_address(void* addr);
    void init(void* addr, uint16_t segment, uint8_t gate, uint8_t priv_level);
    constexpr Gate_Descriptor();
    constexpr Gate_Descriptor(u64 offset, u8 ist, u8 type_attr);


};

struct PACKED ALIGNED(0x1000) IDT {
    Gate_Descriptor entries[256];
};

struct PACKED IDT_descriptor {
    u16 size;
    uint64_t offset;
};

extern IDT k_idt;

void init_interrupts();

extern "C" void loadIDT(IDT_descriptor* IDT_desc);

struct PACKED Interrupt_Stack_Frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t intno;
    uint64_t err;

    // the interrupt stackframe
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

extern "C" uint64_t interrupt_handler(uint64_t rsp);

extern uint64_t isr0;
extern uint64_t isr1;
extern uint64_t isr2;
extern uint64_t isr3;
extern uint64_t isr4;
extern uint64_t isr5;
extern uint64_t isr6;
extern uint64_t isr7;
extern uint64_t isr8;
extern uint64_t isr9;
extern uint64_t isr10;
extern uint64_t isr11;
extern uint64_t isr12;
extern uint64_t isr13;
extern uint64_t isr14;
extern uint64_t isr15;
extern uint64_t isr16;
extern uint64_t isr17;
extern uint64_t isr18;
extern uint64_t isr19;
extern uint64_t isr20;
extern uint64_t isr21;
extern uint64_t isr22;
extern uint64_t isr23;
extern uint64_t isr24;
extern uint64_t isr25;
extern uint64_t isr26;
extern uint64_t isr27;
extern uint64_t isr28;
extern uint64_t isr29;
extern uint64_t isr30;
extern uint64_t isr31;
