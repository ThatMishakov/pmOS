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
    uint8_t cache_disabled            :1;  // Indicates if it is lazilly allocated
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
    uint8_t extra              : 3;
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

inline PML4* pml4_of(UNUSED uint64_t addr)
{
    return ((PML4*)0xfffffffffffff000);
}

inline PML4* pml4()
{
    return ((PML4*)0xfffffffffffff000);
}

inline PDPT* pdpt_of(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9+9);
    addr &= ~(uint64_t)0xfff;
    return ((PDPT*)((uint64_t)01777777777777770000000 | addr));
}

inline PD* pd_of(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9);
    addr &= ~(uint64_t)0xfff;
    return ((PD*)((uint64_t)01777777777770000000000 | addr));
}

inline PT* pt_of(uint64_t addr)
{
    addr = (uint64_t)addr >> (9);
    addr &= ~(uint64_t)0xfff;
    return ((PT*)((uint64_t)01777777770000000000000 | addr));
}

inline PML4E* get_pml4e(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9+9+9);
    addr &= ~(uint64_t)07;
    return ((PML4E*)((uint64_t)01777777777777777770000 | addr));
}

inline PDPTE* get_pdpe(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9+9);
    addr &= ~(uint64_t)07;
    return ((PDPTE*)((uint64_t)01777777777777770000000 | addr));
}

inline PDE* get_pde(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9);
    addr &= ~(uint64_t)0x07;
    return ((PDE*)((uint64_t)01777777777770000000000 | addr));
}

inline PTE* get_pte(uint64_t addr)
{
    addr = (uint64_t)addr >> (9);
    addr &= ~(uint64_t)07;
    return ((PTE*)((uint64_t)01777777770000000000000 | addr));
}

#endif