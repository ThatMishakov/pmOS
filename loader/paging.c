#include <paging.h>
#include "../kernel/common/memory.h"
#include <stdint.h>

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
    if (!pde.present) return 0;

    return 1;
}

void spawn_page(uint64_t addr, Page_Table_Argumments arg)
{

}