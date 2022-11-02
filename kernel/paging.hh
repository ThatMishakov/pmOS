#pragma once
#include "common/memory.h"
#include <stdint.h>
#include "types.hh"
#include "sched.hh"
#include "asm.hh"

#define PAGE_NORMAL  0
#define PAGE_SHARED  1
#define PAGE_SPECIAL 2
#define PAGE_DELAYED 3

typedef struct {
    uint8_t writeable          : 1;
    uint8_t user_access        : 1;
    uint8_t global             : 1;
    uint8_t execution_disabled : 1;
    uint8_t extra              : 3; /* Reserved Shared*/
} Page_Table_Argumments;

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

// Returns physical address of the virt
ReturnStr<uint64_t> phys_addr_of(uint64_t virt);

enum Page_Types {
    NORMAL = 0,
    HUGE_2M = 1,
    HUGE_1G = 2,
    UNALLOCATED = 3,
    LAZY_ALLOC = 4,
    UNKNOWN = 5
};

// Returns page type
Page_Types page_type(uint64_t virtual_addr);

// Invalidade a single page
void invalidade_noerr(uint64_t virtual_addr);

// Frees a page
void free_page(uint64_t page);

// Frees a PML4 of a dead process
void free_pml4(uint64_t pml4);

// Releases cr3
extern "C" void release_cr3(uint64_t cr3);