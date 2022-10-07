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
    uint8_t cache_disable             :1;
    uint8_t accessed                  :1;
    uint8_t ignored                   :1;
    uint8_t reserved                  :1; // Must be 0
    uint8_t avl                       :3;  // Avaliable
    uint8_t pat                       :1;
    uint64_t page_ppn                   :40;
    uint64_t avl2                       :7;
    uint8_t  pk                         :4;
    uint8_t execution_disabled       :1;
} PACKED PML4E;

typedef struct
{
    uint8_t present                   :1;
    uint8_t writeable                 :1;
    uint8_t user_access               :1;
    uint8_t write_through             :1;
    uint8_t cache_disabled            :1;
    uint8_t accessed                  :1;
    uint8_t ignored                   :1;
    uint8_t size                      :1;  // For 1 GB pages 
    uint8_t avl                       :3;  // Avaliable
    uint8_t ignored1                  :1;
    uint64_t page_ppn                 :40;
    uint64_t avl2                     :7;
    uint8_t  pk                       :4;
    uint8_t execution_disabled        :1;
} PACKED PDPTE;

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
    uint8_t avl                       :3;  // Avaliable
    uint8_t ignored                   :1;
    uint64_t page_ppn                 :40;
    uint64_t avl2                     :7;
    uint8_t  pk                       :4;
    uint8_t execution_disabled       :1;
} PACKED PDE;

typedef struct
{
    uint8_t present                   :1;
    uint8_t writeable                 :1;
    uint8_t user_access               :1;
    uint8_t write_through             :1;
    uint8_t cache_disabled            :1;
    uint8_t accessed                  :1;
    uint8_t dirty                     :1;
    uint8_t pat                       :1;
    uint8_t global                    :1; 
    uint8_t avl                       :2;  // Avaliable
    uint8_t ignored                   :1;
    uint64_t page_ppn                 :40;
    uint64_t avl2                     :7;
    uint8_t  pk                       :4;
    uint8_t execution_disabled       :1;
} PACKED PTE;


typedef struct {
    uint8_t writeable          : 1;
    uint8_t user_access        : 1;
    uint8_t global             : 1;
    uint8_t execution_disabled : 1;
    uint8_t   extra              : 3;
} Page_Table_Argumments;

typedef struct {
    PML4E entries[512];
} PACKED ALIGNED(0x1000) PML4;

typedef struct {
    PDPTE entries[512];
} PACKED ALIGNED(0x1000) PDPT;

typedef struct {
    PDE entries[512];
} PACKED ALIGNED(0x1000) PD;

typedef struct {
    PTE entries[512];
} PACKED ALIGNED(0x1000) PT;

#define PAGE_ADDR(page) (page.page_ppn << 12)

#endif