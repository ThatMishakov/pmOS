#ifndef MEM_H
#define MEM_H
#include "../../kernel/common/memory.h"

void bitmap_mark_bit(uint64_t pos, char b);
char bitmap_read_bit(uint64_t pos);
void mark(uint64_t base, uint64_t size, char usable);
void mark_usable(uint64_t base, uint64_t size);
void reserve(uint64_t base, uint64_t size);
void reserve_unal(uint64_t base, uint64_t size);
void bitmap_reserve(uint64_t base, uint64_t size);

void init_mem(unsigned long multiboot_str);

uint64_t alloc_page();


void memclear(void * base, int size_bytes);
void memcpy(char * from, char * to, uint64_t bytes);

#endif