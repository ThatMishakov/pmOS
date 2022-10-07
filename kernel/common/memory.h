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
    uint8_t global                    :1;
    uint8_t size                      :1; // Must be 0
    uint8_t avl                       :3;  // Avaliable
    uint8_t pat                       :1;
    uint64_t page_ppn                   :28;
    uint64_t reserved_1                 :12; // must be 0
    uint64_t avl2                       :7;
    uint8_t  pk                         :4;
    uint8_t execution_disabled       :1;
} PACKED PML4;

typedef struct
{
    uint8_t present                   :1;
    uint8_t writeable                 :1;
    uint8_t user_access               :1;
    uint8_t write_through             :1;
    uint8_t cache_disabled            :1;
    uint8_t accessed                  :1;
    uint8_t global                    :1;
    uint8_t size                      :1; 
    uint8_t avl                       :3;  // Avaliable
    uint8_t pat                       :1;
    uint64_t page_ppn                   :28;
    uint64_t reserved_1                 :12; // must be 0
    uint64_t avl2                       :7;
    uint8_t  pk                         :4;
    uint8_t execution_disabled       :1;
} PACKED PDPT;

typedef struct
{
    uint8_t present                   :1;
    uint8_t writeable                 :1;
    uint8_t user_access               :1;
    uint8_t write_through             :1;
    uint8_t cache_disabled            :1;
    uint8_t accessed                  :1;
    uint8_t global                    :1;
    uint8_t size                      :1; 
    uint8_t avl                       :3;  // Avaliable
    uint8_t pat                       :1;
    uint64_t page_ppn                   :28;
    uint64_t reserved_1                 :12; // must be 0
    uint64_t avl2                       :7;
    uint8_t  pk                         :4;
    uint8_t execution_disabled       :1;
} PACKED PD;



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
    PML4 entries[512];
} PACKED ALIGNED(0x1000) Page_Table;


#endif