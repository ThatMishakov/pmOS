#pragma once
#include "common/memory.h"
#include <stdint.h>
#include "types.hh"

// Tries to assign a page. Returns result
uint64_t get_page(uint64_t virtual_addr, Page_Table_Argumments arg);
uint64_t get_page_zeroed(uint64_t virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
uint64_t map(uint64_t physical_addr, uint64_t virtual_addr, Page_Table_Argumments arg);

// Invalidades a page entry
uint64_t invalidade(uint64_t virtual_addr);

// Constructs a new pml4
ReturnStr<uint64_t> get_new_pml4();

// Release the page
kresult_t release_page_s(uint64_t virtual_address);

// Preallocates an empty page (to be sorted out later by the pagefault manager)
kresult_t alloc_page_lazy(uint64_t virtual_addr, Page_Table_Argumments arg);

kresult_t get_lazy_page(uint64_t virtual_addr);

enum Page_Types {
    NORMAL,
    HUGE_2M,
    HUGE_1G,
    UNALLOCATED,
    LAZY_ALLOC,
    UNKNOWN
};

// Returns page type
Page_Types page_type(int64_t virtual_addr);



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
    return ((PDPT*)((uint64_t)01777777777777777000000 | addr)); // 0177777777777777000000
}

inline PD* pd_of(uint64_t addr)
{
    addr = (uint64_t)addr >> (9+9);
    addr &= ~(uint64_t)0xfff;
    return ((PD*)((uint64_t)01777777777777000000000 | addr));
}

inline PT* pt_of(uint64_t addr)
{
    addr = (uint64_t)addr >> (9);
    addr &= ~(uint64_t)0xfff;
    return ((PT*)((uint64_t)01777777777000000000000 | addr));
}