#pragma once
#include <stdint.h>
void int_to_string(int64_t n, uint8_t base, char* str, int& length);

void uint_to_string(uint64_t n, uint8_t base, char* str, int& length);

void term_write(const char * str, uint64_t length);
uint16_t strlen(const char *str);

void t_print(const char *str,...);