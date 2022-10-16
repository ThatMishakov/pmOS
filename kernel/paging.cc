#include "paging.hh"
#include <stdint.h>
#include "common/memory.h"
#include "mem.hh"
#include "asm.hh"
#include "common/errors.h"
#include "utils.hh"
#include "mem.hh"
#include "free_page_alloc.hh"
#include "utils.hh"
#include "types.hh"

kresult_t get_page(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    ReturnStr<void*> r = palloc.alloc_page();
    if (r.result != SUCCESS) return r.result;

    uint64_t b = map((uint64_t)r.val, virtual_addr, arg);
    if (b != SUCCESS) palloc.free(r.val);
    return b;
}

kresult_t get_page_zeroed(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    kresult_t b = get_page(virtual_addr, arg);
    if (b == SUCCESS) page_clear((void*)virtual_addr);
    return b;
}

kresult_t map(uint64_t physical_addr, uint64_t virtual_addr, Page_Table_Argumments arg)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    //uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    PML4E& pml4e = pml4()->entries[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pdpt_of(virtual_addr));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<uint64_t> p =  palloc.alloc_page_ppn();;
        if (p.result != SUCCESS) return p.result; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pd_of(virtual_addr));
    }

    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pt_of(virtual_addr));
    }

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present or pte.cache_disabled) return ERROR_PAGE_PRESENT;

    pte = {};
    pte.page_ppn = physical_addr/KB(4);
    pte.present = 1;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;  
    return SUCCESS;
}

Page_Types page_type(int64_t virtual_addr)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    //uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    // Check if PDPT is present
    PML4E& pml4e = pml4()->entries[pml4_entry];
    if (not pml4e.present) return Page_Types::UNALLOCATED;

    // Check if PD is present 
    PDPTE& pdpte = pdpt_of(virtual_addr)->entries[pdpt_entry];
    if (pdpte.size) return Page_Types::HUGE_1G;
    if (not pdpte.present) return Page_Types::UNALLOCATED;

    // Check if PT is present
    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return Page_Types::HUGE_2M;
    if (not pde.present) return Page_Types::UNALLOCATED;

    // Check if page is present
    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present) return Page_Types::NORMAL;
    else if (pte.cache_disabled) return Page_Types::LAZY_ALLOC;
    return Page_Types::UNALLOCATED;
}

uint64_t release_page_s(uint64_t virtual_address)
{
    return ERROR_NOT_IMPLEMENTED;
}

ReturnStr<uint64_t> get_new_pml4()
{
    // Get a free page
    ReturnStr<void*> l = palloc.alloc_page();
    uint64_t p = (uint64_t)l.val;

    // Check for errors
    if (l.result != SUCCESS) return {l.result, 0}; // Could not allocate a page

    // Find a free spot and map this page
    ReturnStr<uint64_t> r = get_free_page();
    if (r.result != SUCCESS) return {r.result, 0};
    uint64_t free_page = r.val;

    // Map the page
    Page_Table_Argumments args;
    args.user_access = 0;
    args.writeable = 1;

    // Map the newly created PML4 to some empty space
    kresult_t d = map(p, free_page, args);
    if (d != SUCCESS) { // Check the errors
        palloc.free((void*)p);
        return {d, 0};
    }

    // Flush tlb to apply mapping
    tlb_flush();

    // Copy the last entries into the new page table as they are shared across all processes
    // and recurvicely assign the last page to itself
    ((PML4*)free_page)->entries[509] = pml4()->entries[509];
    ((PML4*)free_page)->entries[510] = pml4()->entries[510];

    ((PML4*)free_page)->entries[511] = PML4E();
    ((PML4*)free_page)->entries[511].present = 1;
    ((PML4*)free_page)->entries[511].user_access = 0;
    ((PML4*)free_page)->entries[511].page_ppn = p/KB(4);

    // Unmap the page
    // TODO: Error checking
    invalidade(free_page);

    // Return the free_page to the pool
    // TODO: Error checking
    release_free_page(free_page);

    return {SUCCESS, p};
}

kresult_t invalidade(uint64_t virtual_addr)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    //uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    // Check if PDPT is present
    PML4E& pml4e = pml4()->entries[pml4_entry];
    if (not pml4e.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if PD is present 
    PDPTE& pdpte = pdpt_of(virtual_addr)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_HUGE_PAGE ;
    if (not pdpte.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if PT is present
    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return ERROR_HUGE_PAGE;
    if (not pde.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if page is present
    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (not pte.present and not pte.cache_disabled) return ERROR_PAGE_NOT_PRESENT;

    // Everything OK
    pte = PTE();
    return SUCCESS;
}

kresult_t alloc_page_lazy(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    //uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    PML4E& pml4e = pml4()->entries[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pdpt_of(virtual_addr));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<uint64_t> p =  palloc.alloc_page_ppn();;
        if (p.result != SUCCESS) return p.result; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pd_of(virtual_addr));
    }

    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pt_of(virtual_addr));
    }

    t_print("%h %h\n", pt_of(virtual_addr), virtual_addr);

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present or pte.cache_disabled) return ERROR_PAGE_PRESENT;

    pte = {};
    // Allocate lazylly
    pte.present = 0;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable; 
    pte.cache_disabled = 1;
    return SUCCESS;
}