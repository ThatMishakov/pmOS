#pragma once
#include <types.hh>

inline u64 read_csr(u32 csr)
{
    u64 value;
    asm volatile("csrrc %0, %1, zero" : "=r"(value) : "=r"(csr));
    return value;
}

inline void write_csr(u32 csr, u64 value)
{
    asm volatile("csrrw zero, %0, %1" : : "r"(csr), "r"(value));
}