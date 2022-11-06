#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.hh"

void int_to_string(int64_t n, uint8_t base, char* str, int& length);

void uint_to_string(uint64_t n, uint8_t base, char* str, int& length);

void term_write(const char * str, uint64_t length);
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