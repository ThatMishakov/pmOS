#include <paging.h>
#include "../kernel/common/memory.h"
#include <stdint.h>
#include <entry.h>
#include <mem.h>

uint64_t after_exec_free_avail;

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
        memclear(*(void ** )pml4e, 4096);
        pml4e->present = 1;
        pml4e->writeable = 1;

        tlb_flush();
    }

    PDPTE* pdpte = get_pdpe(addr);
    if (!pdpte->present) {
        *(uint64_t*)pdpte = alloc_page_t();
        memclear(*(void ** )pdpte, 4096);
        pdpte->present = 1;
        pdpte->writeable = 1;
        tlb_flush();
    } else if (pdpte->size) ;// TODO

    PDE* pde = get_pde(addr);
    if (!pde->present) {
        *(uint64_t*)pde = alloc_page_t();
        memclear(*(void ** )pde, 4096);
        pde->present = 1;
        pde->writeable = 1;
        tlb_flush();
    } else if (pde->size) ; // TODO

    PTE* pte = get_pde(addr);
    if (!pte->present) {
        *(uint64_t*)pte = alloc_page_t();
        memclear(*(void ** )pte, 4096);
        pte->present = 1;
        pte->writeable = arg.writeable;
        pte->execution_disabled = 0;//arg.execution_disabled; // TODO
        tlb_flush();
    }
}

void map(uint64_t addr, uint64_t phys, Page_Table_Argumments arg)
{
    PML4E *pml4e = get_pml4e(addr);
    if (!pml4e->present) {
        *(uint64_t*)pml4e = alloc_page_t();
        memclear(*(void ** )pml4e, 4096);
        pml4e->present = 1;
        pml4e->writeable = 1;

        tlb_flush();
    }

    PDPTE* pdpte = get_pdpe(addr);
    if (!pdpte->present) {
        *(uint64_t*)pdpte = alloc_page_t();
        memclear(*(void ** )pdpte, 4096);
        pdpte->present = 1;
        pdpte->writeable = 1;
        tlb_flush();
    } else if (pdpte->size) ;// TODO

    PDE* pde = get_pde(addr);
    if (!pde->present) {
        *(uint64_t*)pde = alloc_page_t();
        memclear(*(void ** )pde, 4096);
        pde->present = 1;
        pde->writeable = 1;
        tlb_flush();
    } else if (pde->size) ; // TODO

    PTE* pte = get_pde(addr);
    if (!pte->present) {
        pte->page_ppn = phys >> 12;
        pte->present = 1;
        pte->writeable = arg.writeable;
        pte->execution_disabled = 0;//arg.execution_disabled; // TODO
        tlb_flush();
    }
}