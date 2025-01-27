#pragma once

inline unsigned long getCR0()
{
    unsigned long val;
    asm("movl %%cr0, %0" : "=r"(val));
    return val;
}

inline void setCR0(unsigned long val) { asm("movl %0, %%cr0" : : "r"(val)); }

inline unsigned long getCR2()
{
    unsigned long cr4;
    asm("movl %%cr2, %0" : "=r"(cr4));
    return cr4;
}

inline void setCR3(unsigned long val) { asm("movl %0, %%cr3" : : "r"(val)); }

inline unsigned long getCR4()
{
    unsigned long cr4;
    asm("movl %%cr4, %0" : "=r"(cr4));
    return cr4;
}

inline void setCR4(unsigned long val) { asm("movl %0, %%cr4" : : "r"(val)); }

inline u64 read_msr(u32 reg)
{
    ulong eax;
    ulong ecx = reg;
    ulong edx;

    asm volatile("rdmsr" : "=a"(eax), "=d"(edx) : "e"(ecx));

    return eax | ((u64)edx << 32);
}

inline void write_msr(u32 reg, u64 val)
{
    ulong eax = val & 0xffffffff;
    ulong edx = val >> 32;
    ulong ecx = reg;
    asm volatile("wrmsr" ::"a"(eax), "c"(ecx), "d"(edx));
}

inline u64 rdtsc()
{
    ulong eax, edx;
    asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
    return ((u64)edx << 32) | (u64)eax;
}