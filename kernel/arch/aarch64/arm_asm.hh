#pragma once

enum class Barrier : unsigned {
    SY    = 0xF,
    ISH   = 0xB,
    ISHST = 0xA,
    ISHLD = 0x9,
    OSH   = 0x3,
    NSH   = 0x7,
};

template<Barrier B>
inline void dsb()
{
    __builtin_arm_dsb(static_cast<unsigned>(B));
}

template<Barrier B>
inline void dmb()
{
    __builtin_arm_dmb(static_cast<unsigned>(B));
}

inline void isb()
{
    __builtin_arm_isb(0xF);
}

#ifdef __aarch64__
inline u64 mpidr_el1()
{
    u64 val;
    asm volatile("mrs %0, mpidr_el1" : "=r"(val));
    return val;
}
#endif