#ifndef KENREL_MEMORY_H
#define KENREL_MEMORY_H
#include <stdint.h>
#include "types.h"

typedef struct {
    uint64_t base_addr;
    uint64_t size;
} memory_descr;

typedef struct
{
    uint8_t present                   :1;
    uint8_t writeable                 :1;
    uint8_t user_access               :1;
    uint8_t write_through             :1;
    uint8_t cache_disabled            :1;
    uint8_t accessed                  :1;
    uint8_t dirty                     :1;
    uint8_t size                      :1;
    uint8_t global                    :1;
    uint8_t available                   :3;  // Free to use
    uint32_t page_ppn                   :28;
    uint32_t reserved_1                 :12; // must be 0
    uint32_t ignored_1                  :11;
    uint8_t execution_disabled       :1;
} PACKED Page_Table_Entry;

typedef struct {
    uint8_t writeable          : 1;
    uint8_t user_access        : 1;
    uint8_t write_through      : 1;
    uint8_t cache_disabled     : 1;
    uint8_t global             : 1;
    uint8_t execution_disabled : 1;
    uint8_t   extra              : 3;
} PACKED Page_Table_Argumments;

typedef struct {
    Page_Table_Entry entries[512];
} PACKED ALIGNED(0x1000) Page_Table;


#endif