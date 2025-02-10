#pragma once
#include <types.hh>

struct IDT {
    u64 entries[256]{};
};

extern IDT k_idt;

struct IDTR {
    u16 size;
    IDT *offset;
} __attribute__((packed));

inline void loadIDT(IDTR *IDT_desc)
{
    asm volatile ("lidt %0" : : "m" (*IDT_desc));
}