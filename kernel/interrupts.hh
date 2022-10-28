#pragma once
#include <stdint.h>
#include "types.hh"
#include "gdt.hh"

#define INTGATE 0x8e /*  */
#define TRAPGATE 0xee
#define IST 0x01

#define IDT_USER 0b01100000


struct PACKED Gate_Descriptor {
    uint16_t offset0;
    uint16_t segment_sel;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t reserved1;

    void set_address(void* addr);
    void init(void* addr, uint16_t segment, uint8_t gate, uint8_t priv_level);
    constexpr Gate_Descriptor();
    constexpr Gate_Descriptor(u64 offset, u8 ist, u8 type_attr);
};


constexpr Gate_Descriptor::Gate_Descriptor() 
    : Gate_Descriptor(0, 0, 0)
{}

constexpr Gate_Descriptor::Gate_Descriptor(uint64_t offset, uint8_t ist, uint8_t type_attr)
    : offset0(offset & 0xffff),
    segment_sel(R0_CODE_SEGMENT),
    ist(ist),
    attributes(type_attr),
    offset1((offset >> 16) & 0xffff),
    offset2((offset >> 32) & 0xffffffff),
    reserved1(0)
{}

struct IDT {
    Gate_Descriptor entries[256] = {};
} PACKED ALIGNED(0x1000);

struct IDT_descriptor {
    u16 size;
    uint64_t offset;
} PACKED;

extern IDT k_idt;

void init_interrupts();

extern "C" void loadIDT(IDT_descriptor* IDT_desc);
extern "C" void mask_PIC();

struct PACKED RFLAGS_Bits {
    uint8_t carry_flag  :1;
    uint8_t reserved0   :1;
    uint8_t parity_flag :1;
    uint8_t reserved1   :1;
    uint8_t aux_carry   :1;
    uint8_t reserved2   :1;
    uint8_t zero        :1;
    uint8_t sign        :1;
    uint8_t trap        :1;
    uint8_t interrupt_e :1;
    uint8_t direction   :1;
    uint8_t overflow    :1;
    uint8_t iopl        :1;
    uint8_t nested_task :1;
    uint8_t reserved3   :1;
    uint8_t resume      :1;
    uint8_t virtual_m   :1;
    uint8_t allignment  :1;
    uint8_t virtual_i_f :1;
    uint8_t virtual_i_p :1;
    uint8_t it          :1;
};

union RFLAGS {
    uint64_t val = 0;
    RFLAGS_Bits bits;
};

struct PACKED Interrupt_Stackframe {
    uint64_t rip;
    uint64_t cs;
    RFLAGS rflags;
    uint64_t rps;
    uint64_t ss;
};

union Entry_Regs {
    uint64_t rsp;
    Interrupt_Stackframe int_r;
};

struct PACKED Scratch_Regs {
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
    uint64_t rax;
    uint64_t r10;
    uint64_t r11;
};

struct PACKED Preserved_Regs {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
};

enum Entry_Type {
    interrupt = 0,
    syscall   = 1,
};


struct PACKED Task_Regs {
    Entry_Regs e;
    Scratch_Regs scratch_r;
    Preserved_Regs preserved_r;

    Entry_Type t;
};

#define STACK_SIZE KB(16)

struct Stack {
    uint8_t byte[STACK_SIZE];
};

extern "C" void interrupt_handler();

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
extern uint64_t isr32;
extern uint64_t isr33;
extern uint64_t isr34;
extern uint64_t isr35;
extern uint64_t isr36;
extern uint64_t isr37;
extern uint64_t isr38;
extern uint64_t isr39;
extern uint64_t isr40;
extern uint64_t isr41;
extern uint64_t isr42;
extern uint64_t isr43;
extern uint64_t isr44;
extern uint64_t isr45;
extern uint64_t isr46;
extern uint64_t isr47;
extern uint64_t isr127;
extern uint64_t isr128;
extern uint64_t isr202;
