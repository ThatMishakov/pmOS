#pragma once
#include "types.hh"
#include "gdt.hh"
#include <memory/palloc.hh>

void init_interrupts();

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

union RFLAGS {
    u64 numb = 0b1000000000;
    RFLAGS_Bits bits;
};

struct Interrupt_Stackframe {
    u64 rip = 0;
    u64 cs = 0;
    RFLAGS rflags;
    u64 rsp = 0;
    u64 ss = 0;
};

struct Scratch_Regs {
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

struct Preserved_Regs {
    u64 rbx = 0;
    u64 rbp = 0;
    u64 r12 = 0;
    u64 r13 = 0;
    u64 r14 = 0;
    u64 r15 = 0;
};

struct Segment_Offsets {
    u64 fs = 0;
    u64 gs = 0;
};

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1


struct Task_Regs { // 192 bytes
    Scratch_Regs scratch_r; //72 bytes
    Interrupt_Stackframe e; // 40 bytes
    Preserved_Regs preserved_r; // 48 bytes
    Segment_Offsets seg; // 16 bytes
    u64 entry_type = ENTRY_INTERRUPT; // 8 bytes
    u64 int_err = 0;
    u64 intno = 0;
};

#define STACK_SIZE KB(16)

struct Stack {
    u8 byte[STACK_SIZE];

    inline u64* get_stack_top()
    {
        return (u64*)&(byte[STACK_SIZE]);
    }
};

class Kernel_Stack_Pointer {
protected:
    Stack *stack = nullptr;

public:
    Kernel_Stack_Pointer()
    {
        stack = (Stack*)palloc(sizeof(Stack)/4096);
    }

    ~Kernel_Stack_Pointer()
    {
        pfree((void*)stack, sizeof(Stack)/4096);
    }

    inline u64* get_stack_top()
    {
        return stack->get_stack_top();
    }
};


extern "C" void interrupt_handler();
extern "C" void fill_idt();

extern "C" void ret_from_interrupt(void) NORETURN;
extern "C" void ret_from_syscall(void) NORETURN;
extern "C" void ret_from_sysenter(void) NORETURN;

extern void (*return_table[3])(void);

extern "C" void lvt0_int_isr();
extern "C" void lvt1_int_isr();
extern "C" void apic_timer_isr();

extern "C" void dummy_isr();
