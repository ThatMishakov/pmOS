#pragma once
#include <types.hh>

inline void set_pgdl(unsigned long addr)
{
    asm volatile ("csrwr %0, 0x19" :: "r"(addr) : "memory");
}

inline void set_pgdh(unsigned long addr)
{
    asm volatile ("csrwr %0, 0x1a" :: "r"(addr) : "memory");
}

inline u64 get_pgdh()
{
    u64 out;
    asm ("csrrd %0, %1" : "=r"(out) : "i"(0x1A));
    return out;
}

inline void flush_tlb()
{
    asm volatile ("invtlb 0x01, $zero, $zero" ::: "memory");
}

inline void invalidate_kernel_page(void *addr, unsigned asid)
{
    asm volatile ("invtlb 0x6, %0, %1" :: "r"(asid), "r"(addr) : "memory");
}

inline void flush_user_pages()
{
    asm volatile ("invtlb 0x03, $zero, $zero" ::: "memory");
}

inline void invalidate_user_page(void *addr, unsigned asid)
{
    asm volatile ("invtlb 0x5, %0, %1" :: "r"(asid), "r"(addr) : "memory");
}

inline void iocsr_write32(u32 value, u32 address)
{
    asm volatile ("iocsrwr.w %0, %1" :: "r"(value), "r"(address) : "memory");
}

inline void iocsr_write64(u64 value, u32 address)
{
    asm volatile ("iocsrwr.d %0, %1" :: "r"(value), "r"(address) : "memory");
}

inline u32 cpucfg(u32 word)
{
    u32 out;
    asm ("cpucfg %0, %1" : "=r"(out) : "r"(word));
    return out;
}

inline unsigned long timer_value()
{
    unsigned long out;
    asm volatile("csrrd %0, 0x42" : "=r"(out));
    return out;
}

inline void timer_tcfg(unsigned long value)
{
    asm volatile("csrwr %0, 0x41" :: "r"(value));
}