#pragma once
#include <stddef.h>
#include "types.hh"

void int_to_string(i64 n, u8 base, char* str, int& length);

void uint_to_string(u64 n, u8 base, char* str, int& length);

void t_print_bochs(const char *str, ...);

namespace klib { class string; }
void term_write(const klib::string&);

extern "C" size_t strlen(const char *str);

extern "C" int printf(const char *str,...);

inline void halt()
{
    while (1) {
      asm ("hlt");
    }
}

extern "C" void memcpy(char* to, const char* from, size_t size);
extern "C" void *memset(void *str, int c, size_t n);

bool prepare_user_buff_rd(const char* buff, size_t size);

bool prepare_user_buff_wr(char* buff, size_t size);

bool copy_from_user(char* to, const char* from, size_t size);

bool copy_to_user(const char* from, char* to, size_t size);

// Copies a frame (ppn)
void copy_frame(u64 from, u64 to);

void clear_page(u64 phys_addr);

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

template<class A>
const A& max(const A& a, const A& b) noexcept
{
    if (a > b) return a;
    return b;
}

template<class A>
const A& min(const A& a, const A& b) noexcept
{
    return a < b ? a : b;
}

class Logger;

void print_stack_trace(Logger& logger);