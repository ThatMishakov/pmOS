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
        page_clear((void*)pt_of(virtual_addr));
    }

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT;

    pte = {};
    pte.page_ppn = physical_addr/KB(4);
    pte.present = 1;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;  
    pte.avl = arg.extra;
    return SUCCESS;
}

Page_Types page_type(uint64_t virtual_addr)
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
    else if (pte.avl == PAGE_DELAYED) return Page_Types::LAZY_ALLOC;
    return Page_Types::UNALLOCATED;
}

uint64_t release_page_s(uint64_t virtual_address)
{
    PTE& pte = *get_pte(virtual_address);

    switch (pte.avl) {
    case PAGE_NORMAL:
        palloc.free((void*)virtual_address);
    case PAGE_DELAYED:
        invalidade(virtual_address);
        break;
    case PAGE_SPECIAL: // Do nothing
        break;
    default:
        return ERROR_NOT_IMPLEMENTED;
    }

    return SUCCESS;
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

    // Clear it as memory contains rubbish and it will cause weird paging bugs on real machines
    page_clear((void*)free_page);

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
    invalidade_noerr(free_page);

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
    if (pdpte.size) return ERROR_HUGE_PAGE;
    if (not pdpte.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if PT is present
    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return ERROR_HUGE_PAGE;
    if (not pde.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if page is present
    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (not pte.present and pte.avl != PAGE_DELAYED) return ERROR_PAGE_NOT_PRESENT;

    // Everything OK
    pte = PTE();
    invlpg(virtual_addr);

    return SUCCESS;
}

kresult_t alloc_page_lazy(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    PML4E* pml4e = get_pml4e(virtual_addr);
    if (not pml4e->present) {
        *pml4e = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e->page_ppn = p.val;
        pml4e->present = 1;
        pml4e->writeable = 1;
        pml4e->user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr));
    }

    PDPTE* pdpte = get_pdpe(virtual_addr);
    if (pdpte->size) return ERROR_PAGE_PRESENT;
    if (not pdpte->present) {
        *pdpte = {};
        ReturnStr<uint64_t> p =  palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pdpte->page_ppn = p.val;
        pdpte->present = 1;
        pdpte->writeable = 1;
        pdpte->user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr));
    }

    PDE& pde = *get_pde(virtual_addr);
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr));
    }

    PTE& pte = *get_pte(virtual_addr);
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT;

    pte = {};
    // Allocate lazylly
    pte.present = 0;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable; 
    pte.avl = arg.extra;
    pte.avl = PAGE_DELAYED;

    return SUCCESS;
}

kresult_t get_lazy_page(uint64_t virtual_addr)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    //uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present or pte.avl != PAGE_DELAYED) return ERROR_WRONG_PAGE_TYPE;

    // Get an empty page
    ReturnStr<uint64_t> page = palloc.alloc_page_ppn();
    if (page.result != SUCCESS) return page.result;
    pte.page_ppn = page.val;
    pte.present = 1;
    pte.avl = PAGE_NORMAL;

    // Clear the page for security and other reasons
    page_clear((void*)virtual_addr);

    return SUCCESS;
}

bool is_allocated(uint64_t page)
{
    Page_Types p = page_type(page);
    
    return p == Page_Types::NORMAL or p == Page_Types::LAZY_ALLOC;
}

kresult_t transfer_pages(TaskDescriptor* t, uint64_t page_start, uint64_t to_address, uint64_t nb_pages, Page_Table_Argumments pta)
{
    // Check that pages are allocated
    for (uint64_t i = 0; i < nb_pages; ++i) {
        if (not is_allocated(page_start + i*KB(4))) return ERROR_PAGE_NOT_ALLOCATED;
    }

    klib::list<PTE> l;

    // Get pages
    for (uint64_t i = 0; i < nb_pages; ++i) {
        uint64_t p = page_start + i*KB(4);
        l.push_back(*get_pte(p));
    }

    // Save %cr3
    uint64_t cr3 = getCR3();

    // Switch into new process' memory
    setCR3(t->page_table);

    // Map memory
    kresult_t r = SUCCESS;
    auto it = l.begin();
    uint64_t i = 0;
    for (; i < nb_pages and r == SUCCESS; ++i, ++it) {
        r = set_pte(to_address + i*KB(4), *it, pta);
    }

    // If failed, invalidade the pages that succeded
    if (r != SUCCESS)
        for (uint64_t k = 0; k < i; ++i)
            *get_pte(to_address + k*KB(4)) = {};

    // Return old %cr3
    setCR3(cr3);

    // If successfull, invalidate the pages
    if (r == SUCCESS) {
        for (uint64_t i = 0; i < nb_pages; ++i) {
            invalidade_noerr(page_start + i*KB(4));
        }
        setCR3(cr3);
    }

    return r;
}

kresult_t set_pte(uint64_t virtual_addr, PTE pte_n, Page_Table_Argumments arg)
{
    PML4E& pml4e = *get_pml4e(virtual_addr);
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr));
    }

    PDPTE& pdpte = *get_pdpe(virtual_addr);
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<uint64_t> p =  palloc.alloc_page_ppn();;
        if (p.result != SUCCESS) return p.result; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;

        page_clear((void*)pd_of(virtual_addr));
    }

    PDE& pde = *get_pde(virtual_addr);
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<uint64_t> p = palloc.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;

        page_clear((void*)pt_of(virtual_addr));
    }

    PTE& pte = *get_pte(virtual_addr);
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT;

    pte = pte_n;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable; 
    return SUCCESS;
}

ReturnStr<uint64_t> phys_addr_of(uint64_t virt)
{
    PTE* pte = get_pte(virt);

    uint64_t phys = (pte->page_ppn << 12) | (virt & (uint64_t)0xfff);

    // TODO: Error checking
    return {SUCCESS, phys};
}

void invalidade_noerr(uint64_t virtual_addr)
{
    *get_pte(virtual_addr) = PTE();
    invlpg(virtual_addr);
}

extern "C" void release_cr3(uint64_t cr3)
{
    palloc.free((void*)cr3);
}