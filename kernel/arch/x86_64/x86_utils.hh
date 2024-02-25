#pragma once

inline void halt()
{
    while (1) {
      asm ("hlt");
    }
}

static inline void outb(u16 port, u8 data)
{
    asm volatile ("outb %0, %1" :: "a"(data), "Nd"(port));
}

static inline void io_wait()
{
    outb(0x80, 0);
}

static inline u8 inb(u16 port)
{ 
    u8 ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline u64 cpuid(u32 p)
{
    int eax = 0;
    int edx = 0;
    asm volatile ( "cpuid" : "=a"(eax), "=d"(edx) : "0"(p) : "ebx", "ecx" );
    return eax | (u64)edx << 32;
}