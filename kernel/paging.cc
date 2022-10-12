#include "paging.hh"
#include <stdint.h>
#include "common/memory.h"
#include "mem.hh"
#include "asm.hh"

bool get_page(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    void* page = palloc.alloc_page();
    if ((uint64_t)page == (uint64_t)-1) return false;

    bool b = map((uint64_t)page, virtual_addr, arg);
    if (not b) palloc.free(page);
    return b;
}

bool get_page_zeroed(uint64_t virtual_addr, Page_Table_Argumments arg)
{
    bool b = get_page(virtual_addr, arg);
    if (b) page_clear((void*)virtual_addr);
    return b;
}

bool map(uint64_t physical_addr, uint64_t virtual_addr, Page_Table_Argumments arg)
{
    uint64_t addr = virtual_addr;
    addr >>= 12;
    uint64_t page = addr;
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
        pml4e.page_ppn = palloc.alloc_page_ppn();
        if (pml4e.page_ppn == -1) {
            pml4e = {};
            return false;
        }
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pdpt_of(virtual_addr));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr)->entries[pdpt_entry];
    if (pdpte.size) return false;
    if (not pdpte.present) {
        pdpte = {};
        pdpte.page_ppn = palloc.alloc_page_ppn();
        if (pdpte.page_ppn == -1) {
            pdpte = {};
            return false;
        }
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pd_of(virtual_addr));
    }

    PDE& pde = pd_of(virtual_addr)->entries[pdir_entry];
    if (pde.size) return false;
    if (not pde.present) {
        pde = {};
        pde.page_ppn = palloc.alloc_page_ppn();
        if (pde.page_ppn == -1) {
            pde = {};
            return false;
        }
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;

        tlb_flush();

        page_clear((void*)pt_of(virtual_addr));
    }

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present) return false;

    pte = {};
    pte.page_ppn = palloc.alloc_page_ppn();
    if (pte.page_ppn == -1) {
        pte = {};
        return false;
    }
    pte.present = 1;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;
    return true;
}