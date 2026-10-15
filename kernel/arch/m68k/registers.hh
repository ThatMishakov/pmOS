#pragma once

#include <types.hh>

struct M68kRegs {
    u32 d[8] = {0};
    u32 a[8] = {0};
    u32 pc = 0;
    u16 sr = 0;
    u16 padding = 0;
    u32 tcb = 0;

    u16 interrupt_frame[46] = {};

    inline u32 &program_counter() { return pc; }
    inline u32 program_counter() const { return pc; };

    inline u32 &stack_pointer() { return a[7]; }
    inline u32 stack_pointer() const { return a[7]; };

    inline u32 &thread_pointer() { return tcb; }
    inline u32 thread_pointer() const { return tcb; }

    inline u32 &arg1() { return d[0]; }
    inline u32 &arg2() { return d[1]; }
    inline u32 &arg3() { return d[2]; }
};

// Generic Task registers, for all architectures
using Task_Regs = M68kRegs;