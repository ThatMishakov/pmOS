#pragma once

inline unsigned long getCR0()
{
    unsigned long val;
    asm("movl %%cr0, %0" : "=r"(val) :: "memory");
    return val;
}

inline void setCR0(unsigned long val) { asm("movl %0, %%cr0" : : "r"(val) : "memory"); }

inline unsigned long getCR2()
{
    unsigned long cr4;
    asm("movl %%cr2, %0" : "=r"(cr4));
    return cr4;
}

inline void setCR3(unsigned long val) { asm("movl %0, %%cr3" : : "r"(val) : "memory"); }

inline unsigned long getCR3()
{
    unsigned long val;
    asm("movl %%cr3, %0" : "=r"(val) :: "memory");
    return val;
}

inline unsigned long getCR4()
{
    unsigned long cr4;
    asm("movl %%cr4, %0" : "=r"(cr4) :: "memory");
    return cr4;
}

inline void setCR4(unsigned long val) { asm("movl %0, %%cr4" : : "r"(val) : "memory"); }

inline u64 read_msr(u32 reg)
{
    ulong eax;
    ulong ecx = reg;
    ulong edx;

    asm volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(ecx) : "memory");

    return eax | ((u64)edx << 32);
}

inline void write_msr(u32 reg, u64 val)
{
    ulong eax = val & 0xffffffff;
    ulong edx = val >> 32;
    ulong ecx = reg;
    asm volatile("wrmsr" ::"a"(eax), "c"(ecx), "d"(edx) : "memory");
}

inline u64 rdtsc()
{
    ulong eax, edx;
    asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
    return ((u64)edx << 32) | (u64)eax;
}

inline void invlpg(void *m) { asm volatile("invlpg (%0)" : : "r"(m) : "memory"); }

inline u16 str()
{
    u16 val;
    asm volatile("str %0" : "=rm"(val) :: "memory");
    return val;
}