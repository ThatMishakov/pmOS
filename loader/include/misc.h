#ifndef MISC_H
#define MISC_H
#include <stdint.h>

void printc(char c);
void tlb_flush();
void halt();
void clear_page(uint64_t);

#endif