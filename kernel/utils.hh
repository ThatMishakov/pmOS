#pragma once
#include <stddef.h>
#include "types.hh"

void int_to_string(int64_t n, u8 base, char* str, int& length);

void uint_to_string(u64 n, u8 base, char* str, int& length);

void t_print_bochs(const char *str, ...);

void term_write(const char * str, u64 length);
size_t strlen(const char *str);

void t_print(const char *str,...);

extern void printf(const char *str,...);

inline void halt()
{
    while (1) {
      asm ("hlt");
    }
}

void memcpy(const char* from, char* to, size_t size);

kresult_t prepare_user_buff_rd(const char* buff, size_t size);

kresult_t prepare_user_buff_wr(char* buff, size_t size);

kresult_t copy_from_user(const char* from, char* to, size_t size);

kresult_t copy_to_user(const char* from, char* to, size_t size);

// Copies a frame (ppn)
void copy_frame(u64 from, u64 to);

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