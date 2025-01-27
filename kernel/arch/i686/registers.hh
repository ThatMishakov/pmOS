#pragma once

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1
#define ENTRY_SYSENTER  2
#define ENTRY_NESTED    3

struct IA32Regs {
    unsigned long eax {};
    unsigned long ebx {};
    unsigned long ecx {};
    unsigned long edx {};
    unsigned long esi {};
    unsigned long edi {};
    unsigned long ebp {};
    unsigned long esp {};
    unsigned long eip {};
    unsigned long eflags {};
    unsigned long fs {};
    unsigned long gs {};

    unsigned long cs {};

    unsigned entry_type {};
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

    inline bool syscall_pending_restart() const { return saved_entry_type; }

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