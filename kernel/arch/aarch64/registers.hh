#pragma once
#include <types.hh>

struct AArch64Regs {
    u64 x[31] = {0};
    u64 pc = 0;
    u64 sp = 0;
    u64 pstate = 0;
    u64 tpidr_el0 = 0;
    u64 tpidrro_el0 = 0;

    inline u64 &program_counter() { return pc; }
    inline u64 program_counter() const { return pc; };

    inline u64 &stack_pointer() { return sp; }
    inline u64 stack_pointer() const { return sp; };

    inline u64 &thread_pointer() { return tpidr_el0; }
    inline u64 thread_pointer() const { return tpidr_el0; };

    inline u64 syscall_flags() const { return x[0]; }

    inline ulong &arg1() { return x[0]; }
    inline ulong &arg2() { return x[1]; }
    inline ulong &arg3() { return x[2]; }

    inline ulong &syscall_arg1() { return x[0]; }
    inline ulong &syscall_arg2() { return x[1]; }
    inline ulong &syscall_arg3() { return x[2]; }
    inline ulong &syscall_arg4() { return x[3]; }
    inline ulong &syscall_arg5() { return x[4]; }
    inline ulong &syscall_arg6() { return x[5]; }
    
    inline u64 &syscall_retval_low() { return x[0]; }
    inline u64 &syscall_retval_high() { return x[1]; }
};

using Task_Regs = AArch64Regs;
