#include "paging.hh"
#include <kernel/memory.h>
#include "mem.hh"
#include <asm.hh>
#include <kernel/errors.h>
#include <utils.hh>
#include "free_page_alloc.hh"
#include <utils.hh>
#include <types.hh>
#include <kernel/com.h>


kresult_t get_page(u64 virtual_addr, Page_Table_Argumments arg)
{
    ReturnStr<void*> r = palloc.alloc_page();
    if (r.result != SUCCESS) return r.result;

    u64 b = map((u64)r.val, virtual_addr, arg);
    if (b != SUCCESS) palloc.free(r.val);
    return b;
}

kresult_t get_page_zeroed(u64 virtual_addr, Page_Table_Argumments arg)
{
    kresult_t b = get_page(virtual_addr, arg);
    if (b == SUCCESS) page_clear((void*)virtual_addr);
    return b;
}

kresult_t map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg)
{
    u64 addr = virtual_addr;
    addr >>= 12;
    //u64 page = addr;
    u64 ptable_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdir_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pml4_entry = addr & 0x1ff;

    PML4E& pml4e = pml4()->entries[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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
        ReturnStr<u64> p =  palloc.alloc_page_ppn();;
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
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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

Page_Types page_type(u64 virtual_addr)
{
    u64 addr = virtual_addr;
    addr >>= 12;
    //u64 page = addr;
    u64 ptable_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdir_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pml4_entry = addr & 0x1ff;

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

void print_pt(u64 addr)
{
    u64 ptable_entry = addr >> 12;
    u64 pd_e = ptable_entry >> 9;
    u64 pdpe = pd_e >> 9;
    u64 pml4e = pdpe >> 9;
    u64 upper = pml4e >> 9;

    t_print("Paging indexes %h\'%h\'%h\'%h\'%h\n", upper, pml4e&0777, pdpe&0777, pd_e&0777, ptable_entry&0777);
}

u64 release_page_s(u64 virtual_address)
{
    PTE& pte = *get_pte(virtual_address);

    switch (pte.avl) {
    case PAGE_NORMAL:
        palloc.free((void*)(pte.page_ppn << 12));
        FALLTHROUGH;
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

ReturnStr<u64> get_new_pml4()
{
    // Get a free page
    ReturnStr<void*> l = palloc.alloc_page();
    u64 p = (u64)l.val;

    // Check for errors
    if (l.result != SUCCESS) return {l.result, 0}; // Could not allocate a page

    // Find a free spot and map this page
    ReturnStr<u64> r = get_free_page();
    if (r.result != SUCCESS) return {r.result, 0};
    u64 free_page = r.val;

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

kresult_t invalidade(u64 virtual_addr)
{
    u64 addr = virtual_addr;
    addr >>= 12;
    //u64 page = addr;
    u64 ptable_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdir_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdpt_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pml4_entry = addr & 0x1ff;

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

kresult_t alloc_page_lazy(u64 virtual_addr, Page_Table_Argumments arg)
{
    PML4E* pml4e = get_pml4e(virtual_addr);
    if (not pml4e->present) {
        *pml4e = {};
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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
        ReturnStr<u64> p =  palloc.alloc_page_ppn();
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
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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

kresult_t get_lazy_page(u64 virtual_addr)
{
    u64 addr = virtual_addr;
    addr >>= 12;
    //u64 page = addr;
    u64 ptable_entry = addr & 0x1ff;

    PTE& pte = pt_of(virtual_addr)->entries[ptable_entry];
    if (pte.present or pte.avl != PAGE_DELAYED) return ERROR_WRONG_PAGE_TYPE;

    // Get an empty page
    ReturnStr<u64> page = palloc.alloc_page_ppn();
    if (page.result != SUCCESS) return page.result;
    pte.page_ppn = page.val;
    pte.present = 1;
    pte.user_access = 1;
    pte.writeable = 1;
    pte.avl = PAGE_NORMAL;

    // Clear the page for security and other reasons
    page_clear((void*)virtual_addr);

    return SUCCESS;
}

bool is_allocated(u64 page)
{
    Page_Types p = page_type(page);
    
    return p == Page_Types::NORMAL or p == Page_Types::LAZY_ALLOC;
}

kresult_t transfer_pages(TaskDescriptor* t, u64 page_start, u64 to_address, u64 nb_pages, Page_Table_Argumments pta)
{
    // Check that pages are allocated
    for (u64 i = 0; i < nb_pages; ++i) {
        if (not is_allocated(page_start + i*KB(4))) return ERROR_PAGE_NOT_ALLOCATED;
    }

    klib::list<PTE> l;

    // Get pages
    for (u64 i = 0; i < nb_pages; ++i) {
        u64 p = page_start + i*KB(4);
        l.push_back(*get_pte(p));
    }

    // Save %cr3
    u64 cr3 = getCR3();

    // Switch into new process' memory
    setCR3(t->page_table);

    // Map memory
    kresult_t r = SUCCESS;
    auto it = l.begin();
    u64 i = 0;
    for (; i < nb_pages and r == SUCCESS; ++i, ++it) {
        r = set_pte(to_address + i*KB(4), *it, pta);
    }

    // If failed, invalidade the pages that succeded
    if (r != SUCCESS)
        for (u64 k = 0; k < i; ++i)
            *get_pte(to_address + k*KB(4)) = {};

    // Return old %cr3
    setCR3(cr3);

    // If successfull, invalidate the pages
    if (r == SUCCESS) {
        for (u64 i = 0; i < nb_pages; ++i) {
            invalidade_noerr(page_start + i*KB(4));
        }
        setCR3(cr3);
    }

    return r;
}

kresult_t set_pte(u64 virtual_addr, PTE pte_n, Page_Table_Argumments arg)
{
    PML4E& pml4e = *get_pml4e(virtual_addr);
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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
        ReturnStr<u64> p =  palloc.alloc_page_ppn();;
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
        ReturnStr<u64> p = palloc.alloc_page_ppn();
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

ReturnStr<u64> phys_addr_of(u64 virt)
{
    PTE* pte = get_pte(virt);

    u64 phys = (pte->page_ppn << 12) | (virt & (u64)0xfff);

    // TODO: Error checking
    return {SUCCESS, phys};
}

void invalidade_noerr(u64 virtual_addr)
{
    *get_pte(virtual_addr) = PTE();
    invlpg(virtual_addr);
}

extern "C" void release_cr3(u64 cr3)
{
    palloc.free((void*)cr3);
}

void free_pt(u64 page_start)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = page_start + (i << 12);
        PTE* p = get_pte(addr);
        if (p->present) {
            kresult_t result = release_page_s(addr);
            if (result != SUCCESS) {
                t_print("Error %i freeing page %h type %h!\n", result, addr, p->avl);
                print_pt(addr);
                halt();
            }
        }
    }
}

void free_pd(u64 pd_start)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = pd_start + (i << (12 + 9));
        PDE* p = get_pde(addr);
        if (p->size) {
            t_print("Error freeing pages: Huge page!\n");
            halt();
        } else if (p->present) {
            switch (p->avl)
            {
            case NORMAL:
                free_pt(addr);
                break;
            default:
                t_print("Error freeing page: Unknown page type!\n");
                halt();
                break;
            }
        }
    }
}

void free_pdpt(u64 pdp_start)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = pdp_start + (i << (12 + 9 + 9));
        PDPTE* p = get_pdpe(addr);
        if (p->present) {
            free_pd(addr);
            palloc.free((void*)(p->page_ppn << 12));
        }
    }
}

void free_user_pages(u64 page_table)
{
    u64 old_cr3 = getCR3();

    if (old_cr3 != page_table)
        setCR3(page_table);

    for (u64 i = 0; i < KERNEL_ADDR_SPACE; i += (0x01ULL << (12 + 9 + 9 + 9))) {
        PML4E* p = get_pml4e(i);
        if (p->present) {
            free_pdpt(i);
            palloc.free((void*)(p->page_ppn << 12));
        }
    }

    if (old_cr3 != page_table)
        setCR3(old_cr3);
}

kresult_t prepare_user_page(u64 page)
{
    Page_Types type = page_type(page);

    switch (type)
    {
    case NORMAL:
        return SUCCESS;
        break;
    case LAZY_ALLOC:
        return get_lazy_page(page);
        break;
    case UNALLOCATED:
        return ERROR_PAGE_NOT_ALLOCATED;
        break;
    default:
        return ERROR_NOT_IMPLEMENTED;
        break;
    }
}