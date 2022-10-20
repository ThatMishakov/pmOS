#pragma once
#include "common/memory.h"
#include <stdint.h>
#include "types.hh"
#include "sched.hh"

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

// Transfers pages from current process to process t
kresult_t transfer_pages(TaskDescriptor* t, uint64_t page_start, uint64_t to_addr, uint64_t nb_pages, Page_Table_Argumments pta);

// Prepares a page table for the address
kresult_t prepare_pt(uint64_t addr);

// Sets pte with pta at virtual_addr
kresult_t set_pte(uint64_t virtual_addr, PTE pte, Page_Table_Argumments pta);

// Checks if page is allocated
bool is_allocated(uint64_t page);

enum Page_Types {
    NORMAL = 0,
    HUGE_2M = 1,
    HUGE_1G = 2,
    UNALLOCATED = 3,
    LAZY_ALLOC = 4,
    UNKNOWN = 5
};

// Returns page type
Page_Types page_type(int64_t virtual_addr);