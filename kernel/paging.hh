#pragma once
#include "common/memory.h"
#include <stdint.h>
#include "types.hh"

// Returns true if assigned the page successfully
bool get_page(uint64_t virtual_addr, Page_Table_Argumments arg);
bool get_page_zeroed(uint64_t virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
bool map(uint64_t physical_addr, uint64_t virtual_addr, Page_Table_Argumments arg);

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
    addr = (int64_t)addr >> (9+9+9);
    addr &= ~0xfff;
    return ((PDPT*)(0177777777777777000000 | addr));
}

inline PD* pd_of(uint64_t addr)
{
    addr = (int64_t)addr >> (9+9);
    addr &= ~0xfff;
    return ((PD*)(0177777777777000000000 | addr));
}

inline PT* pt_of(uint64_t addr)
{
    addr = (int64_t)addr >> (9);
    addr &= ~0xfff;
    return ((PT*)(0177777777000000000000 | addr));
}