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

struct PACKED RFLAGS {
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
    uint8_t iopl        :2;
    uint8_t nested_task :1;
    uint8_t reserved3   :1;
    uint8_t resume      :1;
    uint8_t virtual_m   :1;
    uint8_t allignment  :1;
    uint8_t virtual_i_f :1;
    uint8_t virtual_i_p :1;
    uint8_t id          :1;
    uint64_t reserved4  :42;
};

struct PACKED Interrupt_Stackframe {
    uint64_t rip = 0;
    uint64_t cs = 0;
    RFLAGS rflags = {};
    uint64_t rsp = 0;
    uint64_t ss = 0;
};

struct PACKED Scratch_Regs {
    uint64_t rdi = 0;
    uint64_t rsi = 0;
    uint64_t rdx = 0;
    uint64_t rcx = 0;
    uint64_t r8 = 0;
    uint64_t r9 = 0;
    uint64_t rax = 0;
    uint64_t r10 = 0;
    uint64_t r11 = 0;
};

struct PACKED Preserved_Regs {
    uint64_t rbx = 0;
    uint64_t rbp = 0;
    uint64_t r12 = 0;
    uint64_t r13 = 0;
    uint64_t r14 = 0;
    uint64_t r15 = 0;
};

enum Entry_Type {
    interrupt = 0,
    syscall   = 1,
};


struct PACKED Task_Regs {
    Scratch_Regs scratch_r;
    Interrupt_Stackframe e;
    Preserved_Regs preserved_r;

    Entry_Type t = interrupt;
};

#define STACK_SIZE KB(16)

struct Stack {
    uint8_t byte[STACK_SIZE];
};

extern "C" void interrupt_handler(uint64_t intno, uint64_t err);
extern "C" void fill_idt();
