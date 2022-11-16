#pragma once
#include "types.hh"
#include "gdt.hh"

#define INTGATE 0x8e /*  */
#define TRAPGATE 0xee
#define IST 0x01

#define IDT_USER 0b01100000


struct PACKED Gate_Descriptor {
    u16 offset0;
    u16 segment_sel;
    u8 ist;
    u8 attributes;
    u16 offset1;
    u32 offset2;
    u32 reserved1;

    void set_address(void* addr);
    void init(void* addr, u16 segment, u8 gate, u8 priv_level);
    constexpr Gate_Descriptor();
    constexpr Gate_Descriptor(u64 offset, u8 ist, u8 type_attr);
};


constexpr Gate_Descriptor::Gate_Descriptor() 
    : Gate_Descriptor(0, 0, 0)
{}

constexpr Gate_Descriptor::Gate_Descriptor(u64 offset, u8 ist, u8 type_attr)
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
    u64 offset;
} PACKED;


void init_interrupts();
void init_kernel_stack();

extern IDT k_idt;
extern "C" void loadIDT(IDT_descriptor* IDT_desc);
void set_idt();

struct PACKED RFLAGS_Bits {
    u8 carry_flag  :1;
    u8 reserved0   :1;
    u8 parity_flag :1;
    u8 reserved1   :1;
    u8 aux_carry   :1;
    u8 reserved2   :1;
    u8 zero        :1;
    u8 sign        :1;
    u8 trap        :1;
    u8 interrupt_e :1;
    u8 direction   :1;
    u8 overflow    :1;
    u8 iopl        :2;
    u8 nested_task :1;
    u8 reserved3   :1;
    u8 resume      :1;
    u8 virtual_m   :1;
    u8 allignment  :1;
    u8 virtual_i_f :1;
    u8 virtual_i_p :1;
    u8 id          :1;
    u64 reserved4  :42;
};

union PACKED RFLAGS {
    u64 numb = 0b1000000000;
    RFLAGS_Bits bits;
};

struct PACKED Interrupt_Stackframe {
    u64 rip = 0;
    u64 cs = 0;
    RFLAGS rflags;
    u64 rsp = 0;
    u64 ss = 0;
};

struct PACKED Scratch_Regs {
    u64 rdi = 0;
    u64 rsi = 0;
    u64 rdx = 0;
    u64 rcx = 0;
    u64 r8 = 0;
    u64 r9 = 0;
    u64 rax = 0;
    u64 r10 = 0;
    u64 r11 = 0;
};

struct PACKED Preserved_Regs {
    u64 rbx = 0;
    u64 rbp = 0;
    u64 r12 = 0;
    u64 r13 = 0;
    u64 r14 = 0;
    u64 r15 = 0;
};

struct PACKED Segment_Offsets {
    u64 fs = 0;
    u64 gs = 0;
};

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1


struct PACKED Task_Regs { // 184 bytes
    Scratch_Regs scratch_r; //72 bytes
    Interrupt_Stackframe e; // 40 bytes
    Preserved_Regs preserved_r; // 48 bytes
    Segment_Offsets seg; // 16 bytes
    u64 entry_type = ENTRY_INTERRUPT; // 8 bytes
};

#define STACK_SIZE KB(16)

struct Stack {
    u8 byte[STACK_SIZE];
};


extern "C" void interrupt_handler(u64 intno, u64 err, Interrupt_Stackframe*);
extern "C" void fill_idt();
