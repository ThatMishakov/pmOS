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
#include "shared_mem.hh"
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>

bool nx_bit_enabled = false;


kresult_t get_page(u64 virtual_addr, Page_Table_Argumments arg)
{
    ReturnStr<void*> r = kernel_pframe_allocator.alloc_page();
    if (r.result != SUCCESS) return r.result;

    u64 b = map((u64)r.val, virtual_addr, arg);
    if (b != SUCCESS) kernel_pframe_allocator.free(r.val);
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

    PML4E& pml4e = pml4(rec_map_index)->entries[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<u64> p =  kernel_pframe_allocator.alloc_page_ppn();;
        if (p.result != SUCCESS) return p.result; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }

    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT;

    pte = {};
    pte.page_ppn = physical_addr/KB(4);
    pte.present = 1;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;  
    pte.avl = arg.extra;
    if (nx_bit_enabled) pte.execution_disabled = arg.execution_disabled;
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
    PML4E& pml4e = pml4(rec_map_index)->entries[pml4_entry];
    if (not pml4e.present) return Page_Types::UNALLOCATED;

    // Check if PD is present 
    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return Page_Types::HUGE_1G;
    if (not pdpte.present) return Page_Types::UNALLOCATED;

    // Check if PT is present
    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size) return Page_Types::HUGE_2M;
    if (not pde.present) return Page_Types::UNALLOCATED;

    // Check if page is present
    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (pte.present) {
        switch (pte.avl) {
        case PAGE_NORMAL:
            return Page_Types::NORMAL;
            break;
        case PAGE_COW:
            return Page_Types::COW;
        case PAGE_SHARED:
            return Page_Types::SHARED;
            break;
        case PAGE_SPECIAL:
            return Page_Types::SPECIAL;
            break;
        default:
            return Page_Types::UNKNOWN;
        }
    } else switch (pte.avl)
    {
    case PAGE_DELAYED:
        return Page_Types::LAZY_ALLOC;
        break;
    case PAGE_NORMAL:
        return Page_Types::UNALLOCATED;
        break;    
    default:
        return Page_Types::UNKNOWN;
        break;
    }

    return Page_Types::UNKNOWN;
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

kresult_t release_page_s(u64 virtual_address, u64 page_table)
{
    PTE& pte = *get_pte(virtual_address, rec_map_index);
    kresult_t p;

    switch (pte.avl) {
    case PAGE_NORMAL:
        kernel_pframe_allocator.free((void*)(pte.page_ppn << 12));
        FALLTHROUGH;
    case PAGE_SPECIAL:
    case PAGE_DELAYED:
        invalidade(virtual_address);
        break;
    case PAGE_SHARED:
    case PAGE_COW:
        p = release_shared(pte.page_ppn << 12, page_table);
        if (p == SUCCESS)
            invalidade(virtual_address);
        return p;
        break;
    default:
        return ERROR_NOT_IMPLEMENTED;
    }

    return SUCCESS;
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
    PML4E& pml4e = pml4(rec_map_index)->entries[pml4_entry];
    if (not pml4e.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if PD is present 
    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_HUGE_PAGE;
    if (not pdpte.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if PT is present
    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size) return ERROR_HUGE_PAGE;
    if (not pde.present) return ERROR_PAGE_NOT_PRESENT;

    // Check if page is present
    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (not pte.present and pte.avl != PAGE_DELAYED) return ERROR_PAGE_NOT_PRESENT;

    // Everything OK
    pte = PTE();
    invlpg(virtual_addr);

    return SUCCESS;
}

static kresult_t alloc_page_lazy_common(u64 virtual_addr, Page_Table_Argumments arg)
{
    PML4E* pml4e = get_pml4e(virtual_addr, rec_map_index);
    if (not pml4e->present) {
        *pml4e = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e->page_ppn = p.val;
        pml4e->present = 1;
        pml4e->writeable = 1;
        pml4e->user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE* pdpte = get_pdpe(virtual_addr, rec_map_index);
    if (pdpte->size) return ERROR_PAGE_PRESENT;
    if (not pdpte->present) {
        *pdpte = {};
        ReturnStr<u64> p =  kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pdpte->page_ppn = p.val;
        pdpte->present = 1;
        pdpte->writeable = 1;
        pdpte->user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = *get_pde(virtual_addr, rec_map_index);
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }

    return SUCCESS;
}

kresult_t alloc_page_lazy(u64 virtual_addr, Page_Table_Argumments arg, u64 flags)
{
    kresult_t r = alloc_page_lazy_common(virtual_addr, arg);
    if (r != SUCCESS) return r;

    PTE& pte = *get_pte(virtual_addr, rec_map_index);
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT;

    pte = {};
    // Allocate lazylly
    pte.present = 0;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable; 
    if (nx_bit_enabled) pte.execution_disabled = arg.execution_disabled;
    pte.avl = PAGE_DELAYED;
    pte.page_ppn = flags;

    return SUCCESS;
}

static void lazy_expand_noerr(u64 from, u64 to, PTE old_pte)
{
    if (is_in_kernel_space(from) != is_in_kernel_space(to))
        return;

    Page_Types to_page_type = page_type(to);

    if (to_page_type != Page_Types::UNALLOCATED)
        return;

    Page_Table_Argumments arg;
    arg.execution_disabled = old_pte.execution_disabled;
    arg.extra = 0;
    arg.global = 0;
    arg.user_access = old_pte.user_access;
    arg.writeable = old_pte.writeable;

    kresult_t r = alloc_page_lazy_common(to, arg);
    if (r != SUCCESS) return;

    PTE& pte = *get_pte(to, rec_map_index);
    pte = old_pte;
}

kresult_t get_lazy_page(u64 virtual_addr)
{
    virtual_addr &= ~0xfffUL;
    u64 addr = virtual_addr;
    addr >>= 12;
    //u64 page = addr;
    u64 ptable_entry = addr & 0x1ff;

    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (pte.present or pte.avl != PAGE_DELAYED) return ERROR_WRONG_PAGE_TYPE;

    u64 flags = pte.page_ppn;
    if (flags & LAZY_FLAG_GROW_UP) {
        lazy_expand_noerr(virtual_addr, virtual_addr + 4096, pte);
    }
    if (flags & LAZY_FLAG_GROW_DOWN) {
        lazy_expand_noerr(virtual_addr, virtual_addr - 4096, pte);
    }

    // Get an empty page
    ReturnStr<u64> page = kernel_pframe_allocator.alloc_page_ppn();
    if (page.result != SUCCESS) return page.result;
    pte.page_ppn = page.val;
    pte.present = 1;
    pte.user_access = 1;
    pte.writeable = pte.writeable;
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


kresult_t atomic_transfer_pages(const klib::shared_ptr<TaskDescriptor>& from, const klib::shared_ptr<TaskDescriptor> t, u64 page_start, u64 to_address, u64 nb_pages, Page_Table_Argumments pta)
{
    Auto_Lock_Scope_Double scope_lock(from->page_table.shared_str->lock, t->page_table.shared_str->lock);

    // Check that pages are allocated
    for (u64 i = 0; i < nb_pages; ++i) {
        if (not is_allocated(page_start + i*KB(4))) return ERROR_PAGE_NOT_ALLOCATED;
    }

    klib::list<PTE> l;

    // Get pages
    for (u64 i = 0; i < nb_pages; ++i) {
        u64 p = page_start + i*KB(4);
        l.push_back(*get_pte(p, rec_map_index));
    }

    // Save %cr3
    u64 cr3 = getCR3();

    // Switch into new process' memory
    setCR3(t->page_table.get_cr3());

    // Map memory
    kresult_t r = SUCCESS;
    auto it = l.begin();
    u64 i = 0;
    for (; i < nb_pages; ++i, ++it) {
        u8 page_type = (*it).avl;
        if (page_type == PAGE_COW) {
            r = register_shared((*it).page_ppn << 12, t->page_table.get_cr3());
            if (r != SUCCESS)
                break;

            if (not pta.writeable)
                pta.extra = PAGE_SHARED;
            else
                pta.extra = page_type;
        } else if (page_type == PAGE_SHARED) {
            r = register_shared((*it).page_ppn << 12, t->page_table.get_cr3());
            if (r != SUCCESS)
                break;

            pta.extra = page_type;
        } else {
            pta.extra = page_type;
        }

        r = set_pte(to_address + i*KB(4), *it, pta);
        if (r != SUCCESS)
            break;
    }

    // If failed, invalidade the pages that succeded
    if (r != SUCCESS)
        for (u64 k = 0; k < i; ++k) {
            PTE* p = get_pte(page_start + i*KB(4), rec_map_index);
            if (p->avl == PAGE_COW or p->avl == PAGE_SHARED)
                release_shared(p->avl, t->page_table.get_cr3());

            *get_pte(to_address + k*KB(4), rec_map_index) = {};
        }

    // Return old %cr3
    setCR3(cr3);

    // If successfull, invalidate the pages
    if (r == SUCCESS) {
        for (u64 i = 0; i < nb_pages; ++i) {
            PTE* p = get_pte(page_start + i*KB(4), rec_map_index);
            if (p->avl == PAGE_COW or p->avl == PAGE_SHARED)
                release_shared(p->avl, from->page_table.get_cr3());
            invalidade_noerr(page_start + i*KB(4));
        }
        setCR3(cr3);
    }

    return r;
}


kresult_t atomic_share_pages(const klib::shared_ptr<TaskDescriptor>& t, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta)
{
    klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;

    klib::vector<klib::pair<PTE, bool>> l;

    kresult_t p = SUCCESS;
    // Share pages

    Auto_Lock_Scope_Double scope_lock(t->page_table.shared_str->lock, current_task->page_table.shared_str->lock);

    u64 z = 0;
    for (; z < nb_pages and p == SUCCESS; ++z) {
        ReturnStr<klib::pair<PTE,bool>> r = share_page(page_start + z*KB(4), current_task->page_table.get_cr3());
        p = r.result;
        if (p == SUCCESS) l.push_back(r.val);
    }

    // Skip TLB flushes on error
    if (p != SUCCESS) goto fail;

    {
    // Save %cr3
    u64 cr3 = getCR3();

    // Switch into new process' memory
    setCR3(t->page_table.get_cr3());

    pta.extra = PAGE_SHARED;

    auto it = l.begin();
    u64 i = 0;
    for (; i < nb_pages and p == SUCCESS; ++i, ++it) {
        PTE pte = (*it).first;
        if (nx_bit_enabled) pte.execution_disabled = pta.execution_disabled;
        pte.writeable = pta.writeable;
        p = register_shared(pte.page_ppn << 12, t->pid);
        if (p == SUCCESS)
            p = set_pte(to_addr + i*KB(4), pte, pta);
    }

    // Return everything back on error
    if (p != SUCCESS)
        for (u64 k = 0; k < i; ++k)
            if (l[k].second)
                release_page_s(to_addr + i*KB(4), t->pid);
            

    // Return old %cr3
    setCR3(cr3);
    }
fail:
    if (p != SUCCESS) {
        u64 size = l.size();
        for (u64 k = 0; k < size; ++k)
            if (l[k].second)
                unshare_page(page_start + k*KB(4), current_task->page_table.get_cr3());
    }

    return p;
}

ReturnStr<klib::pair<PTE, bool>> share_page(u64 virtual_addr, u64 owner_page_table)
{
    kresult_t r = ERROR_GENERAL;
    klib::pair<PTE, bool> k = {PTE(), false};
    Page_Types p = page_type(virtual_addr);

    switch (p) {
    case Page_Types::UNALLOCATED:
        r = ERROR_PAGE_NOT_ALLOCATED;
        break;
    case Page_Types::SPECIAL:
        r = ERROR_SPECIAL_PAGE;
        break;
    case Page_Types::HUGE_2M:
    case Page_Types::HUGE_1G:
        r = ERROR_HUGE_PAGE; // TODO
        break;
    case Page_Types::LAZY_ALLOC:
    {
        r = get_lazy_page(virtual_addr);
        if (r != SUCCESS) break;
    }
        FALLTHROUGH;
    case Page_Types::NORMAL:
    {
        PTE* pte = get_pte(virtual_addr, rec_map_index);
        r = register_shared(pte->page_ppn << 12, owner_page_table);
        if (r != SUCCESS)
            break;
        pte->avl = PAGE_SHARED;
        k.first = *pte;
        k.second = true;
    }
        break;
    case Page_Types::COW:
    case Page_Types::SHARED:
    {
        r = SUCCESS;
        k = {*get_pte(virtual_addr, rec_map_index), false};
    }
        break;
    default:
        break;
    }

    return {r, k};
}

kresult_t set_pte(u64 virtual_addr, PTE pte_n, Page_Table_Argumments arg)
{
    PML4E& pml4e = *get_pml4e(virtual_addr, rec_map_index);
    if (not pml4e.present) {
        pml4e = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE& pdpte = *get_pdpe(virtual_addr, rec_map_index);
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<u64> p =  kernel_pframe_allocator.alloc_page_ppn();;
        if (p.result != SUCCESS) return p.result; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;

        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = *get_pde(virtual_addr, rec_map_index);
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS) return p.result; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;

        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }

    PTE& pte = *get_pte(virtual_addr, rec_map_index);
    if (pte.present or pte.avl == PAGE_DELAYED) return ERROR_PAGE_PRESENT; // TODO!

    pte = pte_n;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;
    pte.avl = arg.extra;
    if (nx_bit_enabled) pte.execution_disabled = arg.execution_disabled;
    return SUCCESS;
}

kresult_t unshare_page(u64 virtual_addr, u64 pid)
{
    PTE* pte = get_pte(virtual_addr, rec_map_index);
    u8 page_type = pte->avl;
    u64 page = pte->page_ppn << 12;

    if (page_type == PAGE_SHARED) {
        if (nb_owners(page) == 1) { // Only owner
            kresult_t r = release_shared(page, pid);
            if (r != SUCCESS)
                return r;

            pte->avl = PAGE_NORMAL;
            return SUCCESS;
        } else { // Copy page
            ReturnStr<u64> k = kernel_pframe_allocator.alloc_page_ppn();
            if (k.result != SUCCESS) return k.result;

            kresult_t r = release_shared(page, pid);
            if (r != SUCCESS) {
                kernel_pframe_allocator.free_ppn(k.val);
                return r;
            }

            copy_frame(pte->page_ppn, k.val);

            pte->page_ppn = k.val;
            pte->avl = PAGE_NORMAL;
            return SUCCESS;
        }
    } else if (page_type == PAGE_COW) {
        return ERROR_NOT_IMPLEMENTED;
    } else {
        return ERROR_PAGE_NOT_SHARED;
    }

    return ERROR_GENERAL;
}

ReturnStr<u64> phys_addr_of(u64 virt)
{
    PTE* pte = get_pte(virt, rec_map_index);

    u64 phys = (pte->page_ppn << 12) | (virt & (u64)0xfff);

    // TODO: Error checking
    return {SUCCESS, phys};
}

void invalidade_noerr(u64 virtual_addr)
{
    *get_pte(virtual_addr, rec_map_index) = PTE();
    invlpg(virtual_addr);
}

void free_pt(u64 page_start, u64 page_table)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = page_start + (i << 12);
        PTE* p = get_pte(addr, rec_map_index);
        if (p->present) {
            kresult_t result = release_page_s(addr, page_table);
            if (result != SUCCESS) {
                t_print_bochs("Error %i freeing page %h type %h!\n", result, addr, p->avl);
                print_pt(addr);
                halt();
            }
        }
    }
}

void free_pd(u64 pd_start, u64 page_table)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = pd_start + (i << (12 + 9));
        PDE* p = get_pde(addr, rec_map_index);
        if (p->size) {
            t_print("Error freeing pages: Huge page!\n");
            halt();
        } else if (p->present) {
            switch (p->avl)
            {
            case NORMAL:
                free_pt(addr, page_table);
                break;
            default:
                t_print("Error freeing page: Unknown page type!\n");
                halt();
                break;
            }
        }
    }
}

void free_pdpt(u64 pdp_start, u64 page_table)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = pdp_start + (i << (12 + 9 + 9));
        PDPTE* p = get_pdpe(addr, rec_map_index);
        if (p->present) {
            free_pd(addr, page_table);
            kernel_pframe_allocator.free((void*)(p->page_ppn << 12));
        }
    }
}

void free_user_pages(u64 page_table)
{
    u64 old_cr3 = getCR3();

    if (old_cr3 != page_table)
        setCR3(page_table);

    for (u64 i = 0; i < KERNEL_ADDR_SPACE; i += (0x01ULL << (12 + 9 + 9 + 9))) {
        PML4E* p = get_pml4e(i, rec_map_index);
        if (p->present) {
            free_pdpt(i, page_table);
            kernel_pframe_allocator.free((void*)(p->page_ppn << 12));
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
    case SPECIAL:
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

Page_Table Page_Table::init_from_phys(u64 cr3)
{
    Page_Table t;

    t.pml4_phys = (PML4 *)cr3;
    t.shared_str = new shared_info();

    return t;
}

Page_Table Page_Table::get_new_page_table()
{
    // Get a refcount structure
    klib::unique_ptr<shared_info> refc = klib::make_unique<shared_info>();

    // Get a free page
    ReturnStr<void*> l = kernel_pframe_allocator.alloc_page();
    u64 p = (u64)l.val;

    // TODO: Throw exception
    // Check for errors
    if (l.result != SUCCESS) return Page_Table();

    // Find a free spot and map this page
    ReturnStr<u64> r = global_free_page.get_free_page();
    u64 free_page = r.val;

    // Map the page
    Page_Table_Argumments args;
    args.user_access = 0;
    args.writeable = 1;

    // Map the newly created PML4 to some empty space
    // TODO: Exceptions
    map(p, free_page, args);

    // Clear it as memory contains rubbish and it will cause weird paging bugs on real machines
    page_clear((void*)free_page);

    // Copy the last entries into the new page table as they are shared across all processes
    // and recurvicely assign the last page to itself
    // ((PML4*)free_page)->entries[509] = pml4(rec_map_index)->entries[509];
    ((PML4*)free_page)->entries[511] = pml4(rec_map_index)->entries[511];

    ((PML4*)free_page)->entries[rec_map_index] = PML4E();
    ((PML4*)free_page)->entries[rec_map_index].present = 1;
    ((PML4*)free_page)->entries[rec_map_index].user_access = 0;
    ((PML4*)free_page)->entries[rec_map_index].page_ppn = p/KB(4);

    // Unmap the page
    // TODO: Error checking
    invalidade_noerr(free_page);

    // Return the free_page to the pool
    // TODO: Error checking
    global_free_page.release_free_page(free_page);

    Page_Table t;

    t.pml4_phys = (PML4*)p;
    t.shared_str = refc.release();

    return t;
}

Page_Table& Page_Table::operator=(Page_Table&& t) noexcept
{
    this->~Page_Table();

    this->pml4_phys = t.pml4_phys;
    this->shared_str = t.shared_str;

    t.pml4_phys = nullptr;
    t.shared_str = nullptr;
    
    return *this;
}

Page_Table::Page_Table(Page_Table&& n) noexcept: pml4_phys(n.pml4_phys), shared_str(n.shared_str)
{
    n.pml4_phys = nullptr;
    n.shared_str = nullptr;
}

Page_Table::~Page_Table()
{
    if (shared_str != nullptr) {
        u64 new_refcount = __atomic_add_fetch(&(shared_str->refcount), -1, 0);

        if (new_refcount == 0) {
            delete shared_str;

            free_user_pages((u64)pml4_phys);
            kernel_pframe_allocator.free((void*)pml4_phys);
        }
    }
}