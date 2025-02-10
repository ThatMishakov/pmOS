#pragma once
#include <types.hh>

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1
#define ENTRY_SYSENTER  2
#define ENTRY_NESTED    3

struct IA32Regs {
    unsigned long eax {}; // 0
    unsigned long ebx {}; // 4
    unsigned long ecx {}; // 8
    unsigned long edx {}; // 12
    unsigned long esi {}; // 16
    unsigned long edi {}; // 20
    unsigned long ebp {}; // 24
    unsigned long esp {}; // 28
    unsigned long eip {}; // 32
    unsigned long eflags {(1 << 9) | (1 << 21)}; // 36
    unsigned long cs {}; // 40
    unsigned long fs {}; // 44
    unsigned long gs {}; // 48

    unsigned entry_type {}; // 52
    unsigned saved_entry_type {};

    // This is slightly stupid, but whatever
    unsigned int_err {};

    inline unsigned long &program_counter() { return eip; }
    inline unsigned long &stack_pointer() { return esp; }

    inline void request_syscall_restart()
    {
        saved_entry_type = entry_type;
        entry_type       = ENTRY_NESTED;
    }
    inline void clear_syscall_restart()
    {
        entry_type       = saved_entry_type;
        saved_entry_type = 0;
    }

    inline bool syscall_pending_restart() const { return entry_type == ENTRY_NESTED; }

    inline unsigned get_cs() const { return cs; }
    inline unsigned long xbp() const { return ebp; }

    inline unsigned long &arg1() { return edi; }
    inline unsigned long &arg2() { return esi; }
    inline unsigned long &arg3() { return edx; }

    void set_iopl(unsigned iopl)
    {
        eflags &= ~0x3000;
        eflags |= iopl << 12;
    }

    inline ulong &thread_pointer() { return fs; }
    inline ulong thread_pointer() const { return fs; }
    inline ulong &global_pointer() { return gs; }
    inline ulong global_pointer() const { return gs; }
};

using Task_Regs = IA32Regs;