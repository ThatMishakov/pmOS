#pragma once
#include "types.hh"
#include "gdt.hh"
#include <memory/palloc.hh>
#include <lib/memory.hh>

void init_interrupts();

void set_idt();

/// @brief %RFLAGS register bits
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

/// @brief Scratch registers of the process.
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

/// Preserved registers of the process. By the conventions, kernel should not change those
/// upon switching to kernel unless the process is aware about it.
struct Preserved_Regs {
    u64 rbx = 0;
    u64 rbp = 0;
    u64 r12 = 0;
    u64 r13 = 0;
    u64 r14 = 0;
    u64 r15 = 0;
};

/// Segment registers. Even though the segmentation is dead, on x86_64 these are still usefull and %gs is used for storing the thread-local data offset.
struct Segment_Offsets {
    u64 fs = 0;
    u64 gs = 0;
};

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1
#define ENTRY_SYSENTER  2
#define ENTRY_NESTED    3

/// Registers of the process. These are saved upon entering the kernel and restored upon returning.
struct Task_Regs { // 208 bytes
    Scratch_Regs scratch_r; //72 bytes
    Interrupt_Stackframe e; // 40 bytes
    Preserved_Regs preserved_r; // 48 bytes
    Segment_Offsets seg; // 16 bytes
    u64 entry_type = ENTRY_INTERRUPT; // 8 bytes
    u64 int_err = 0; // 8
    u64 error_instr = 0; // 8
    u64 saved_entry_type = 0; // 8
};

extern "C" void interrupt_handler();

// Different ways of returning from kernel. The right function is chosen upon entering the kernel and
// is subsequently called upon returning from kernel back to the userspace. This is neede because stacks are
// not switched upon task switches. These functions are called at the ends on the kernel entry points, defined
// in assemnly
extern "C" void ret_from_interrupt(void) NORETURN; ///< Assembly function used for returning from an interrupt
extern "C" void ret_from_syscall(void) NORETURN; ///< Assembly function used for returning from SYSCALL instruction
extern "C" void ret_from_sysenter(void) NORETURN; ///< Assembly function used for returning from SYSENTER instruction
extern "C" void ret_repeat_syscall(void) NORETURN; ///< Assembly funtion used for returning from a repeated syscall.
                                                   ///< This function does not actually return to the userspace, but restores the previous context
                                                   ///< and reexecutes the last syscall that the process was issuing before blocking.

extern void (*return_table[4])(void);

extern "C" void lvt0_int_isr();
extern "C" void lvt1_int_isr();
extern "C" void apic_timer_isr();

extern "C" void dummy_isr();
