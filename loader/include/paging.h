#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include "../../kernel/common/memory.h"

char is_present(PML4* table, uint64_t addr);

void get_page(uint64_t addr, Page_Table_Argumments arg);

#endif