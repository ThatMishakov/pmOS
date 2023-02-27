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
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <kern_logger/kern_logger.hh>

bool nx_bit_enabled = false;

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

Check_Return_Str check_if_allocated_and_set_flag(u64 virtual_addr, u8 flag, Page_Table_Argumments arg)
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
        if (p.result != SUCCESS) 
            return {p.result, false, 0}; 
        pml4e.page_ppn = p.val;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return {ERROR_PAGE_PRESENT, 0, 0};
    if (not pdpte.present) {
        pdpte = {};
        ReturnStr<u64> p =  kernel_pframe_allocator.alloc_page_ppn();;
        if (p.result != SUCCESS) return 
            {p.result, false, 0}; 
        pdpte.page_ppn = p.val;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size)
        return {ERROR_PAGE_PRESENT, 0, 0};
    if (not pde.present) {
        pde = {};
        ReturnStr<u64> p = kernel_pframe_allocator.alloc_page_ppn();
        if (p.result != SUCCESS)
            return {p.result, 0, false}; 
        pde.page_ppn = p.val;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }


    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (pte.present) return {SUCCESS, pte.avl, true};

    Check_Return_Str ret {SUCCESS, pte.avl, false};

    pte.avl |= flag;

    return ret;
}

void free_page(u64 addr, u64 cr3)
{
    u64 old_cr3 = getCR3();

    if (cr3 != old_cr3)
        setCR3(cr3);

    do {
        PML4E& pml4e = *get_pml4e(addr, rec_map_index);
        if (not pml4e.present)
            break;

        PDPTE& pdpte = *get_pdpe(addr, rec_map_index);
        if (not pdpte.present)
            break;

        PDE& pde = *get_pde(addr, rec_map_index);
        if (not pde.present)
            break;

        PTE& pte = *get_pte(addr, rec_map_index);
        if (pte.present and not (pte.avl & PAGING_FLAG_NOFREE))
            kernel_pframe_allocator.free((void*)(pte.page_ppn << 12));

        pte = PTE();
    } while (false);

    if (cr3 != old_cr3)
        setCR3(old_cr3);

    return;
}


kresult_t kernel_get_page(u64 virtual_addr, Page_Table_Argumments arg)
{
    ReturnStr<void*> r = kernel_pframe_allocator.alloc_page();
    if (r.result != SUCCESS) return r.result;

    u64 b = map((u64)r.val, virtual_addr, arg);
    if (b != SUCCESS) kernel_pframe_allocator.free(r.val);
    return b;
}


void print_pt(u64 addr)
{
    u64 ptable_entry = addr >> 12;
    u64 pd_e = ptable_entry >> 9;
    u64 pdpe = pd_e >> 9;
    u64 pml4e = pdpe >> 9;
    u64 upper = pml4e >> 9;

    global_logger.printf("Paging indexes %h\'%h\'%h\'%h\'%h\n", upper, pml4e&0777, pdpe&0777, pd_e&0777, ptable_entry&0777);
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

kresult_t release_page_s(PTE& pte)
{
    if (pte.present and not (pte.avl & PAGING_FLAG_NOFREE))
            kernel_pframe_allocator.free((void*)(pte.page_ppn << 12));

    pte = PTE();

    return SUCCESS;
}



// kresult_t atomic_transfer_pages(const klib::shared_ptr<TaskDescriptor>& from, const klib::shared_ptr<TaskDescriptor> t, u64 page_start, u64 to_address, u64 nb_pages, Page_Table_Argumments pta)
// {
//     Auto_Lock_Scope_Double scope_lock(from->page_table.shared_str->lock, t->page_table.shared_str->lock);

//     // Check that pages are allocated
//     for (u64 i = 0; i < nb_pages; ++i) {
//         if (not is_allocated(page_start + i*KB(4))) return ERROR_PAGE_NOT_ALLOCATED;
//     }

//     klib::list<PTE> l;

//     // Get pages
//     for (u64 i = 0; i < nb_pages; ++i) {
//         u64 p = page_start + i*KB(4);
//         l.push_back(*get_pte(p, rec_map_index));
//     }

//     // Save %cr3
//     u64 cr3 = getCR3();

//     // Switch into new process' memory
//     setCR3(t->page_table.get_cr3());

//     // Map memory
//     kresult_t r = SUCCESS;
//     auto it = l.begin();
//     u64 i = 0;
//     for (; i < nb_pages; ++i, ++it) {
//         u8 page_type = (*it).avl;
//         if (page_type == PAGE_COW) {
//             r = register_shared((*it).page_ppn << 12, t->page_table.get_cr3());
//             if (r != SUCCESS)
//                 break;

//             if (not pta.writeable)
//                 pta.extra = PAGE_SHARED;
//             else
//                 pta.extra = page_type;
//         } else if (page_type == PAGE_SHARED) {
//             r = register_shared((*it).page_ppn << 12, t->page_table.get_cr3());
//             if (r != SUCCESS)
//                 break;

//             pta.extra = page_type;
//         } else {
//             pta.extra = page_type;
//         }

//         r = set_pte(to_address + i*KB(4), *it, pta);
//         if (r != SUCCESS)
//             break;
//     }

//     // If failed, invalidade the pages that succeded
//     if (r != SUCCESS)
//         for (u64 k = 0; k < i; ++k) {
//             PTE* p = get_pte(page_start + i*KB(4), rec_map_index);
//             if (p->avl == PAGE_COW or p->avl == PAGE_SHARED)
//                 release_shared(p->avl, t->page_table.get_cr3());

//             *get_pte(to_address + k*KB(4), rec_map_index) = {};
//         }

//     // Return old %cr3
//     setCR3(cr3);

//     // If successfull, invalidate the pages
//     if (r == SUCCESS) {
//         for (u64 i = 0; i < nb_pages; ++i) {
//             PTE* p = get_pte(page_start + i*KB(4), rec_map_index);
//             if (p->avl == PAGE_COW or p->avl == PAGE_SHARED)
//                 release_shared(p->avl, from->page_table.get_cr3());
//             invalidade_noerr(page_start + i*KB(4));
//         }
//         setCR3(cr3);
//     }

//     return r;
// }


// kresult_t atomic_share_pages(const klib::shared_ptr<TaskDescriptor>& t, u64 page_start, u64 to_addr, u64 nb_pages, Page_Table_Argumments pta)
// {
//     klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;

//     klib::vector<klib::pair<PTE, bool>> l;

//     kresult_t p = SUCCESS;
//     // Share pages

//     Auto_Lock_Scope_Double scope_lock(t->page_table.shared_str->lock, current_task->page_table.shared_str->lock);

//     u64 z = 0;
//     for (; z < nb_pages and p == SUCCESS; ++z) {
//         ReturnStr<klib::pair<PTE,bool>> r = share_page(page_start + z*KB(4), current_task->page_table.get_cr3());
//         p = r.result;
//         if (p == SUCCESS) l.push_back(r.val);
//     }

//     // Skip TLB flushes on error
//     if (p != SUCCESS) goto fail;

//     {
//     // Save %cr3
//     u64 cr3 = getCR3();

//     // Switch into new process' memory
//     setCR3(t->page_table.get_cr3());

//     pta.extra = PAGE_SHARED;

//     auto it = l.begin();
//     u64 i = 0;
//     for (; i < nb_pages and p == SUCCESS; ++i, ++it) {
//         PTE pte = (*it).first;
//         if (nx_bit_enabled) pte.execution_disabled = pta.execution_disabled;
//         pte.writeable = pta.writeable;
//         p = register_shared(pte.page_ppn << 12, t->pid);
//         if (p == SUCCESS)
//             p = set_pte(to_addr + i*KB(4), pte, pta);
//     }

//     // Return everything back on error
//     if (p != SUCCESS)
//         for (u64 k = 0; k < i; ++k)
//             if (l[k].second)
//                 release_page_s(to_addr + i*KB(4), t->pid);
            

//     // Return old %cr3
//     setCR3(cr3);
//     }
// fail:
//     if (p != SUCCESS) {
//         u64 size = l.size();
//         for (u64 k = 0; k < size; ++k)
//             if (l[k].second)
//                 unshare_page(page_start + k*KB(4), current_task->page_table.get_cr3());
//     }

//     return p;
// }


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
            release_page_s(*get_pte(addr, rec_map_index));
        }
    }
}

void free_pd(u64 pd_start, u64 page_table)
{
    for (u64 i = 0; i < 512; ++i) {
        u64 addr = pd_start + (i << (12 + 9));
        PDE* p = get_pde(addr, rec_map_index);
        if (p->size) {
            global_logger.printf("Error freeing pages: Huge page!\n");
            halt();
        } else if (p->present) {
            free_pt(addr, page_table);
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

klib::shared_ptr<Page_Table> Page_Table::init_from_phys(u64 cr3)
{
    klib::shared_ptr<Page_Table> t = klib::unique_ptr<Page_Table>(new Page_Table());
    t->pml4_phys = (PML4 *)cr3;
    t->insert_global_page_tables();

    return t;
}

klib::shared_ptr<Page_Table> Page_Table::create_empty_page_table()
{
    klib::shared_ptr<Page_Table> new_table = klib::unique_ptr<Page_Table>(new Page_Table());


    // Get a free page
    ReturnStr<void*> l = kernel_pframe_allocator.alloc_page();
    u64 p = (u64)l.val;

    // TODO: Throw exception
    // Check for errors
    if (l.result != SUCCESS) return nullptr;

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

    new_table->pml4_phys = (PML4*)p;

    new_table->insert_global_page_tables();

    return new_table;
}

Page_Table::~Page_Table()
{
    paging_regions.clear();

    free_user_pages((u64)pml4_phys);
    kernel_pframe_allocator.free((void*)pml4_phys);
    takeout_global_page_tables();
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

ReturnStr<u64> Page_Table::atomic_create_normal_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 pattern)
{
    Auto_Lock_Scope scope_lock(lock);

    ReturnStr<u64> new_region_start = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    if (new_region_start.result != SUCCESS)
        return new_region_start;

    u64 start_addr = new_region_start.val;

    paging_regions.insert({start_addr,
        klib::make_unique<Private_Normal_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), (u64)pml4_phys, access, pattern
        )});

    return new_region_start;
}

ReturnStr<u64> Page_Table::atomic_create_managed_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, klib::shared_ptr<Port> t)
{
    Auto_Lock_Scope scope_lock(lock);

    ReturnStr<u64> new_region_start = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    if (new_region_start.result != SUCCESS)
        return new_region_start;

    u64 start_addr = new_region_start.val;

    paging_regions.insert({start_addr,
        klib::make_unique<Private_Managed_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), (u64)pml4_phys, access, klib::forward<klib::shared_ptr<Port>>(t)
        )});

    return new_region_start;
}

ReturnStr<u64> Page_Table::atomic_create_phys_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 phys_addr_start)
{
    Auto_Lock_Scope scope_lock(lock);

    ReturnStr<u64> new_region_start = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    if (new_region_start.result != SUCCESS)
        return new_region_start;

    u64 start_addr = new_region_start.val;

    paging_regions.insert({start_addr,
        klib::make_unique<Phys_Mapped_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), (u64)pml4_phys, access, phys_addr_start
        )});

    return new_region_start;
}


void free_pages_range(u64 addr_start, u64 size, u64 cr3)
{
    // TODO: Inefficient!
    for (u64 i = 0; i < size; i += 4096) {
        free_page(addr_start+i, cr3);
    }
}

ReturnStr<u64> Page_Table::find_region_spot(u64 desired_start, u64 size, bool fixed)
{
    u64 end = desired_start + size;

    bool region_ok = desired_start != 0;

    if (region_ok) {
        auto it = paging_regions.get_smaller_or_equal(desired_start);
        if (it != paging_regions.end() and it->second->is_in_range(desired_start))
            region_ok = false;
    }

    if (region_ok) {
        auto it = paging_regions.lower_bound(desired_start);
        if (it != paging_regions.end() and it->first < end)
            region_ok = false;
    }

    if (region_ok) {
        return {SUCCESS, desired_start};
    } else {
        if (fixed)
            return {ERROR_REGION_OCCUPIED, 0};

        u64 addr = 0x1000;
        auto it = paging_regions.begin();
        while (it != paging_regions.end()) {
            u64 end = addr + size;

            if (it->first > end and end <= KERNEL_ADDR_SPACE)
                return {SUCCESS, addr};

            addr = it->second->addr_end();
            ++it;
        }

        if (addr + size <= KERNEL_ADDR_SPACE)
            return {SUCCESS, addr};

        return {ERROR_NO_FREE_REGION, 0};
    }
}

kresult_t Page_Table::prepare_user_page(u64 virt_addr, unsigned access_type, const klib::shared_ptr<TaskDescriptor>& task)
{
    auto it = paging_regions.get_smaller_or_equal(virt_addr);

    if (it == paging_regions.end() or not it->second->is_in_range(virt_addr))
        return ERROR_OUT_OF_RANGE;

    Generic_Mem_Region &reg = *it->second;

    return reg.prepare_page(access_type, virt_addr, task);
}

void Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase_if_exists(this->id);
}

void Page_Table::insert_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.insert({id, weak_from_this()});
}

Page_Table::page_table_map Page_Table::global_page_tables;
Spinlock Page_Table::page_table_index_lock;