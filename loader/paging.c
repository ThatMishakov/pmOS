#include <paging.h>
#include <kernel/memory.h>
#include <stdint.h>
#include <entry.h>
#include <mem.h>

uint64_t after_exec_free_avail;
char nx_enabled = 0;

uint64_t alloc_page_t()
{
    uint64_t p = after_exec_free_avail;
    after_exec_free_avail += 4096;
    return p;
}

void get_page(uint64_t addr, Page_Table_Argumments arg)
{
    PML4E *pml4e = get_pml4e(addr);
    if (!pml4e->present) {
        *(uint64_t*)pml4e = alloc_page_t();
        pml4e->present = 1;
        pml4e->writeable = 1;
        memclear(pdpt_of(addr), 4096);
    }

    PDPTE* pdpte = get_pdpe(addr);
    if (!pdpte->present) {
        *(uint64_t*)pdpte = alloc_page_t();
        pdpte->present = 1;
        pdpte->writeable = 1;
        memclear(pd_of(addr), 4096);
    } //else if (pdpte->size) ;// TODO

    PDE* pde = get_pde(addr);
    if (!pde->present) {
        *(uint64_t*)pde = alloc_page_t();
        pde->present = 1;
        pde->writeable = 1;
        memclear(pt_of(addr), 4096);
    } //else if (pde->size) ; // TODO

    PTE* pte = get_pte(addr);
    if (!pte->present) {
        pte->page_ppn = alloc_page() >> 12;
        pte->present = 1;
        pte->writeable = arg.writeable;
        pte->avl = arg.extra;
        if (nx_enabled) pte->execution_disabled = arg.execution_disabled;
        else pte->execution_disabled = 0;
    }
}

void map(uint64_t addr, uint64_t phys, Page_Table_Argumments arg)
{
    PML4E *pml4e = get_pml4e(addr);
    if (!pml4e->present) {
        *(uint64_t*)pml4e = alloc_page_t();
        pml4e->present = 1;
        pml4e->writeable = 1;
        memclear(pdpt_of(addr), 4096);
    }

    PDPTE* pdpte = get_pdpe(addr);
    if (!pdpte->present) {
        *(uint64_t*)pdpte = alloc_page_t();
        pdpte->present = 1;
        pdpte->writeable = 1;
        memclear(pd_of(addr), 4096);
    } //else if (pdpte->size) ;// TODO

    PDE* pde = get_pde(addr);
    if (!pde->present) {
        *(uint64_t*)pde = alloc_page_t();
        pde->present = 1;
        pde->writeable = 1;
        memclear(pt_of(addr), 4096);
    } //else if (pde->size) ; // TODO

    PTE* pte = get_pte(addr);
    if (!pte->present) {
        pte->page_ppn = phys >> 12;
        pte->present = 1;
        pte->writeable = arg.writeable;
        pte->avl = arg.extra;
        if (nx_enabled) pte->execution_disabled = arg.execution_disabled;
        else pte->execution_disabled = 0;
    }
}