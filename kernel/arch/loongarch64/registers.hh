#pragma once

#include <errno.h>
#include <types.hh>

struct LoongArch64Regs {
    u64 pc = 0;
    u64 ra = 0;
    u64 tp = 0;
    u64 sp = 0;
    u64 a0 = 0;
    u64 a1 = 0;
    u64 a2 = 0;
    u64 a3 = 0;
    u64 a4 = 0;
    u64 a5 = 0;
    u64 a6 = 0;
    u64 a7 = 0;
    u64 t0 = 0;
    u64 t1 = 0;
    u64 t2 = 0;
    u64 t3 = 0;
    u64 t4 = 0;
    u64 t5 = 0;
    u64 t6 = 0;
    u64 t7 = 0;
    u64 t8 = 0;
    u64 r21 = 0;
    u64 fp = 0;
    u64 s0 = 0;
    u64 s1 = 0;
    u64 s2 = 0;
    u64 s3 = 0;
    u64 s4 = 0;
    u64 s5 = 0;
    u64 s6 = 0;
    u64 s7 = 0;
    u64 s8 = 0;


    u64 syscall_restart = 0;

    inline void request_syscall_restart() { syscall_restart = 1; }
    inline void clear_syscall_restart() { syscall_restart = 0; }
    inline bool syscall_pending_restart() const { return syscall_restart; }

    inline u64 &program_counter() { return pc; }
    inline u64 program_counter() const { return pc; };

    inline u64 &stack_pointer() { return sp; }
    inline u64 stack_pointer() const { return sp; };

    inline u64 &thread_pointer() { return tp; }
    inline u64 thread_pointer() const { return tp; };

    inline unsigned long &arg1() { return a0; }
    inline unsigned long &arg2() { return a1; }
    inline unsigned long &arg3() { return a2; }

    inline u64 syscall_flags() const { return a0; }
    inline u64 syscall_number() const { return a0 & 0xff; }

    // Get syscall arguments, starting from 1
    inline u64 &syscall_arg1() { return a1; }
    inline u64 &syscall_arg2() { return a2; }
    inline u64 &syscall_arg3() { return a3; }
    inline u64 &syscall_arg4() { return a4; }
    inline u64 &syscall_arg5() { return a5; }
    inline u64 &syscall_arg6() { return a6; }

    // Get syscall return value registers
    inline u64 &syscall_retval_low() { return a0; }
    inline u64 &syscall_retval_high() { return a1; }
};

// Generic Task registers, for all architectures
using Task_Regs = LoongArch64Regs;