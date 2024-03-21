#pragma once
#include <types.hh>

inline void get_scause_stval(u64* scause, u64* stval)
{
    asm volatile("csrr %0, scause" : "=r"(*scause));
    asm volatile("csrr %0, stval" : "=r"(*stval));
}

constexpr u32 FFLAGS = 0x001;
constexpr u32 FRM = 0x002;
constexpr u32 FCSR = 0x003;