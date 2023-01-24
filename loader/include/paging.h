#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <kernel/memory.h>

#define REC_MAP_INDEX 511

extern char nx_enabled;

typedef struct {
    uint8_t writeable          : 1;
    uint8_t user_access        : 1;
    uint8_t global             : 1;
    uint8_t execution_disabled : 1;
    uint8_t extra              : 3; /* Reserved Shared*/
} Page_Table_Argumments;

char is_present(PML4* table, uint64_t addr);

void get_page(uint64_t addr, Page_Table_Argumments arg);

void map(uint64_t addr, uint64_t phys, Page_Table_Argumments arg);

#endif