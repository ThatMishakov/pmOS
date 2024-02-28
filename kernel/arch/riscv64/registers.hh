#pragma once

#include <types.hh>

// RiscV64 registers
// This structure holds the registers of the RiscV64 architecture.
// It is typically used to save the state of userspace processes
// during the system call and interrupt handling
// Since x0 is hardwired to 0, store pc in its place
// The rest of the registers are save in the order of their numbers
struct RiscV64Regs {
    u64 pc = 0;
    u64 ra = 0;
    u64 sp = 0;
    u64 gp = 0;
    u64 tp = 0;
    u64 t0 = 0;
    u64 t1 = 0;
    u64 t2 = 0;
    u64 s0 = 0;
    u64 s1 = 0;
    u64 a0 = 0;
    u64 a1 = 0;
    u64 a2 = 0;
    u64 a3 = 0;
    u64 a4 = 0;
    u64 a5 = 0;
    u64 a6 = 0;
    u64 a7 = 0;
    u64 s2 = 0;
    u64 s3 = 0;
    u64 s4 = 0;
    u64 s5 = 0;
    u64 s6 = 0;
    u64 s7 = 0;
    u64 s8 = 0;
    u64 s9 = 0;
    u64 s10 = 0;
    u64 s11 = 0;
    u64 t3 = 0;
    u64 t4 = 0;
    u64 t5 = 0;
    u64 t6 = 0;

    // 1 if the syscall should be repeated
    // Store it here as on other architectures (x86) the entry type is stored here
    u64 syscall_restart = 0;

    // Do the as functions, for it to be the same for all architectures

    // Program counter
    inline u64 &program_counter() { return pc; }
    inline u64 program_counter() const { return pc; };
    // Stack pointer
    inline u64 &stack_pointer() { return sp; }
    inline u64 stack_pointer() const { return sp; };
    
    inline u64 &thread_pointer() { return tp; }
    inline u64 thread_pointer() const { return tp; };
    inline u64 &global_pointer() { return gp; }
    inline u64 global_pointer() const { return gp; };

    // Register holding syscall number
    // TODO: Other operating systems usually use a7 for the syscall number,
    //       consider changing this...
    inline u64& syscall_number() { return a0; }
    inline u64 syscall_number() const { return a0; }

    // Get syscall arguments, starting from 1
    inline u64& syscall_arg1() { return a1; }
    inline u64& syscall_arg2() { return a2; }
    inline u64& syscall_arg3() { return a3; }
    inline u64& syscall_arg4() { return a4; }
    inline u64& syscall_arg5() { return a5; }
    inline u64& syscall_arg6() { return a6; }

    // Get syscall return value registers
    inline u64& syscall_retval_low()  { return a0; }
    inline u64& syscall_retval_high() { return a1; }

    // Registers holding function arguments
    inline u64& arg1() { return a0; }
    inline u64& arg2() { return a1; }
    inline u64& arg3() { return a2; }
    inline u64& arg4() { return a3; }

    inline void request_syscall_restart() { syscall_restart = 1; }
    inline void clear_syscall_restart() { syscall_restart = 0; }
};

// Generic Task registers, for all architectures
using Task_Regs = RiscV64Regs;