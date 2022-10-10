#ifndef MEM_H
#define MEM_H
#include "../../kernel/common/memory.h"

void init_mem(unsigned long multiboot_str);
extern memory_descr *memm;
extern int *memm_index;
uint64_t alloc_page();
void memclear(void * base, int size_bytes);
void memcpy(char * from, char * to, uint64_t bytes);

#endif