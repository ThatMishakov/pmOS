#pragma once

static inline void dsb_ishst()
{
    asm volatile("dsb ishst" ::: "memory");
}

static inline void dsb_ish()
{
    asm volatile("dsb ish" ::: "memory");
}

static inline void isb()
{
    asm volatile("isb" ::: "memory");
}