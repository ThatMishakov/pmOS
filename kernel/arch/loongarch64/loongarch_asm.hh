#pragma once
#include <types.hh>

template<unsigned addr>
inline auto csrwr(auto value)
{
    asm volatile ("csrwr %0, %1" : "+r"(value) : "i"(addr));
    return value;
}

template<unsigned addr>
inline u64 csrrd64()
{
    u64 result;
    asm volatile ("csrrd %0, %1" : "=r"(result) : "i"(addr));
    return result;
}

template<unsigned addr>
inline u32 csrrd32()
{
    u32 result;
    asm volatile ("csrrd %0, %1" : "=r"(result) : "i"(addr));
    return result;
}

template<unsigned addr>
inline u32 csrxchg32(u32 value, u32 mask)
{
    asm volatile ("csrxchg %0, %1, %2" : "+r"(value) : "r"(mask), "i"(addr));
    return value;
}

inline void set_pgdl(unsigned long addr)
{
    csrwr<0x19>(addr);
}

inline void set_pgdh(unsigned long addr)
{
    csrwr<0x1a>(addr);
}

inline u64 get_pgdh()
{
    u64 result;
    asm volatile ("csrrd %0, 0x1a" : "=r"(result));
    return result;
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

inline void iocsr_write8(u8 value, u32 address)
{
    asm volatile ("iocsrwr.b %0, %1" :: "r"(value), "r"(address) : "memory");
}

inline void iocsr_write32(u32 value, u32 address)
{
    asm volatile ("iocsrwr.w %0, %1" :: "r"(value), "r"(address) : "memory");
}

inline void iocsr_write64(u64 value, u32 address)
{
    asm volatile ("iocsrwr.d %0, %1" :: "r"(value), "r"(address) : "memory");
}

inline u32 iocsr_read32(u32 address)
{
    u32 value;
    asm volatile ("iocsrrd.w %0, %1" : "=r"(value) : "r"(address));
    return value;
}

inline u64 iocsr_read64(u32 address)
{
    u64 value;
    asm volatile ("iocsrrd.d %0, %1" : "=r"(value) : "r"(address));
    return value;
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
    csrwr<0x41>(value);
}