#include <paging.h>
#include "../kernel/common/memory.h"
#include <stdint.h>
#include <entry.h>
#include <mem.h>

char is_present(PML4* table, uint64_t addr)
{
    addr >>= 12;
    uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    PML4E pml4e = table->entries[pml4_entry];
    if (!pml4e.present) return 0;

    PDPTE pdpte = ((PDPT*)PAGE_ADDR(pml4e))->entries[pdpt_entry];
    if (!pdpte.present) return 0;
    if (pdpte.size) return 1; // 1 GB page

    PDE pde = ((PD*)PAGE_ADDR(pdpte))->entries[pdir_entry];
    if (!pde.present) return 0;
    if (pde.size) return 1; // 2MB page

    PTE pte = ((PT*)PAGE_ADDR(pde))->entries[ptable_entry];
    if (!pte.present) return 0;

    return 1;
}

void get_page(uint64_t addr, Page_Table_Argumments arg)
{
    addr >>= 12;
    uint64_t page = addr;
    uint64_t ptable_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdir_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    uint64_t pml4_entry = addr & 0x1ff;

    print_str("pml4i ");
    print_hex(pml4_entry);
    print_str(" pdpti ");
    print_hex(pdpt_entry);
    print_str(" pdiri ");
    print_hex(pdir_entry);
    print_str(" ptablei ");
    print_hex(ptable_entry);
    print_str("\n");

    PML4E *pml4e = &g_pml4.entries[pml4_entry];
    if (!pml4e->present) {
        *(uint64_t*)pml4e = alloc_page();
        memclear(*(void ** )pml4e, 4096);
        pml4e->present = 1;
        pml4e->writeable = 1;
    }

    PDPTE* pdpte = &((PDPT*)PAGE_ADDR((*pml4e)))->entries[pdpt_entry];
    if (!pdpte->present) {
        *(uint64_t*)pdpte = alloc_page();
        memclear(*(void ** )pdpte, 4096);
        pdpte->present = 1;
        pdpte->writeable = 1;
    } else if (pdpte->size) ;// TODO

    PDE* pde = &((PD*)PAGE_ADDR((*pdpte)))->entries[pdir_entry];
    if (!pde->present) {
        *(uint64_t*)pde = alloc_page();
        memclear(*(void ** )pde, 4096);
        pde->present = 1;
        pde->writeable = 1;
    } else if (pde->size) ; // TODO

    PTE* pte = &((PT*)PAGE_ADDR((*pde)))->entries[ptable_entry];
    if (!pte->present) {
        *(uint64_t*)pte = alloc_page();
        memclear(*(void ** )pte, 4096);
        pte->present = 1;
        pte->writeable = arg.writeable;
        pte->execution_disabled = 0;//arg.execution_disabled; // TODO
    }
}