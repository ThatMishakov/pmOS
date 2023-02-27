#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

// Returns 1 if str1 starts with str2, 0 otherwise
char str_starts_with(char* str1, char* str2);

void cls(void);
void putchar(int c);

void lprintf(const char *str, ...);

int uint_to_string(unsigned long int n, uint8_t base, char* str);
#endif
