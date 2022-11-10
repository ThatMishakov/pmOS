#pragma once
#include <kernel/memory.h>
#include <types.hh>
#include <processes/sched.hh>
#include <asm.hh>
#include <lib/pair.hh>

#define PAGE_NORMAL  0
#define PAGE_SHARED  1
#define PAGE_SPECIAL 2
#define PAGE_DELAYED 3
#define PAGE_COW     4

struct Page_Table_Argumments {
    u8 writeable          : 1 = 0;
    u8 user_access        : 1 = 0;
    u8 global             : 1 = 0;
    u8 execution_disabled : 1 = 0;
    u8 extra              : 3 = 0;
};

// Tries to assign a page. Returns result
u64 get_page(u64 virtual_addr, Page_Table_Argumments arg);
u64 get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg);

// Return true if mapped the page successfully
u64 map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg);

// Invalidades a page entry
u64 invalidade(u64 virtual_addr);

// Constructs a new pml4
ReturnStr<u64> get_new_pml4();

// Release the page
kresult_t release_page_s(u64 virtual_address, u64 pid);

// Preallocates an empty page (to be sorted out later by the pagefault manager)
kresult_t alloc_page_lazy(u64 virtual_addr, Page_Table_Argumments arg);

kresult_t get_lazy_page(u64 virtual_addr);

// Transfers pages from current process to process t
kresult_t transfer_pages(TaskDescriptor* t, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta);

// Sharess pages from current process with process t
kresult_t share_pages(TaskDescriptor* t, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta);

// Prepares a page table for the address
kresult_t prepare_pt(u64 addr);

// Sets pte with pta at virtual_addr
kresult_t set_pte(u64 virtual_addr, PTE pte, Page_Table_Argumments pta);

// Checks if page is allocated
bool is_allocated(u64 page);

// Returns physical address of the virt
ReturnStr<u64> phys_addr_of(u64 virt);

enum Page_Types {
    NORMAL = 0,
    HUGE_2M = 1,
    HUGE_1G = 2,
    UNALLOCATED = 3,
    LAZY_ALLOC = 4,
    SHARED = 5,
    COW = 6,
    UNKNOWN = 7
};

// Returns page type
Page_Types page_type(u64 virtual_addr);

// Invalidade a single page
void invalidade_noerr(u64 virtual_addr);

// Frees a page
void free_page(u64 page, u64 pid);

// Frees a PML4 of a dead process
void free_pml4(u64 pml4, u64 pid);

// Releases cr3
extern "C" void release_cr3(u64 cr3);

// Frees user pages
void free_user_pages(u64 page_table, u64 pid);

// Prepares a user page for kernel reading or writing
kresult_t prepare_user_page(u64 page);

// Makes the page shared and returns its PTE
ReturnStr<klib::pair<PTE, bool>> share_page(u64 virtual_addr, u64 pid);

// Unshares a page and makes PID its only owner
kresult_t unshare_page(u64 virtual_addr, u64 pid);