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
#include "temp_mapper.hh"

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
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pml4e.page_ppn = p;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        u64 p =  kernel_pframe_allocator.alloc_page_ppn();;

        pdpte.page_ppn = p;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pde.page_ppn = p;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }

    PTE& pte = pt_of(virtual_addr, rec_map_index)->entries[ptable_entry];
    if (pte.present) return ERROR_PAGE_PRESENT;

    pte = {};
    pte.page_ppn = physical_addr/KB(4);
    pte.present = 1;
    pte.user_access = arg.user_access;
    pte.writeable = arg.writeable;  
    pte.avl = arg.extra;
    if (nx_bit_enabled) pte.execution_disabled = arg.execution_disabled;
    return SUCCESS;
}

u64 rec_get_pt_ppn(u64 virt_addr)
{
    return get_pde(virt_addr, rec_map_index)->page_ppn;
}

PT* rec_prepare_pt_for(u64 virtual_addr, Page_Table_Argumments arg)
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
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pml4e.page_ppn = p;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        page_clear((void*)pdpt_of(virtual_addr, rec_map_index));
    }

    PDPTE& pdpte = pdpt_of(virtual_addr, rec_map_index)->entries[pdpt_entry];
    if (pdpte.size) return nullptr;
    if (not pdpte.present) {
        pdpte = {};
        u64 p =  kernel_pframe_allocator.alloc_page_ppn();

        pdpte.page_ppn = p;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        page_clear((void*)pd_of(virtual_addr, rec_map_index));
    }

    PDE& pde = pd_of(virtual_addr, rec_map_index)->entries[pdir_entry];
    if (pde.size) return nullptr;
    if (not pde.present) {
        pde = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pde.page_ppn = p;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        page_clear((void*)pt_of(virtual_addr, rec_map_index));
    }

    return pt_of(virtual_addr, rec_map_index);
}

u64 x86_4level_Page_Table::get_page_frame(u64 virt_addr)
{
    u64 cr3 = getCR3();
    u64 local_cr3 = reinterpret_cast<u64>(pml4_phys);
    if (cr3 != local_cr3)
        setCR3(local_cr3);

    PTE& pte = *get_pte(virt_addr, rec_map_index);
    u64 page_frame = pte.page_ppn << 12;

    if (cr3 != local_cr3)
        setCR3(cr3);

    return page_frame;
}

bool x86_4level_Page_Table::is_mapped(u64 virt_addr) const
{
    bool allocated = false;

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);

    do {
        mapper.map((u64)pml4_phys);
        x86_PAE_Entry* pml4e = &mapper.ptr[pml4_index(virt_addr)];
        if (not pml4e->present)
            break;

        mapper.map((u64)pml4e->page_ppn << 12);
        x86_PAE_Entry* pdpte = &mapper.ptr[pdpt_index(virt_addr)];
        if (not pdpte->present)
            break;

        mapper.map((u64)pdpte->page_ppn << 12);
        x86_PAE_Entry* pde = &mapper.ptr[pd_index(virt_addr)];
        if (not pde->present)
            break;

        mapper.map((u64)pde->page_ppn << 12);
        x86_PAE_Entry* pte = &mapper.ptr[pt_index(virt_addr)];
        allocated = pte->present;
    } while (false);

    return allocated;
}


void x86_4level_Page_Table::invalidate(u64 virt_addr, bool free)
{
    bool invalidated = false;
    
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);

    do {
        mapper.map((u64)pml4_phys);
        x86_PAE_Entry* pml4e = &mapper.ptr[pml4_index(virt_addr)];
        if (not pml4e->present)
            break;

        mapper.map((u64)pml4e->page_ppn << 12);
        x86_PAE_Entry* pdpte = &mapper.ptr[pdpt_index(virt_addr)];
        if (not pdpte->present)
            break;

        mapper.map((u64)pdpte->page_ppn << 12);
        x86_PAE_Entry* pde = &mapper.ptr[pd_index(virt_addr)];
        if (not pde->present)
            break;

        mapper.map((u64)pde->page_ppn << 12);
        x86_PAE_Entry* pte = &mapper.ptr[pt_index(virt_addr)];
        if (not pte->present)
            break;

        if (free)
            pte->clear_auto();
        else
            pte->clear_nofree();

        invalidated = true;
    } while (false);

    if (invalidated)
        invalidate_tlb(virt_addr);
}

void x86_Page_Table::invalidate_tlb(u64 page)
{
    // TODO: IPI
    invlpg(page);
}

Page_Table::Check_Return_Str x86_4level_Page_Table::check_if_allocated_and_set_flag(u64 virtual_addr, u8 flag, Page_Table_Argumments arg)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);

    mapper.map((u64)pml4_phys);
    x86_PAE_Entry* pml4e = &mapper.ptr[pml4_index(virtual_addr)];
    if (not pml4e->present) {
        *pml4e = x86_PAE_Entry();

        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pml4e->page_ppn = p;
        pml4e->present = 1;
        pml4e->writeable = 1;
        pml4e->user_access = arg.user_access;

        clear_page(p << 12);
    }

    mapper.map((u64)pml4e->page_ppn << 12);
    x86_PAE_Entry* pdpte = &mapper.ptr[pdpt_index(virtual_addr)];
    if (not pdpte->present) {
        *pdpte = x86_PAE_Entry();

        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pdpte->page_ppn = p;
        pdpte->present = 1;
        pdpte->writeable = 1;
        pdpte->user_access = arg.user_access;

        clear_page(p << 12);
    }

    mapper.map((u64)pdpte->page_ppn << 12);
    x86_PAE_Entry* pde = &mapper.ptr[pd_index(virtual_addr)];
    if (pde->pat_size) // Huge page
        throw(Kern_Exception(ERROR_PAGE_PRESENT, "huge page in check_if_allocated_and_set_flag"));

    if (not pde->present) {
        *pde = x86_PAE_Entry();

        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pde->page_ppn = p;
        pde->present = 1;
        pde->writeable = 1;
        pde->user_access = arg.user_access;

        clear_page(p << 12);
    }


    mapper.map((u64)pde->page_ppn << 12);
    x86_PAE_Entry* pte = &mapper.ptr[pt_index(virtual_addr)];

    if (pte->present)
        return {pte->avl, true};

    Check_Return_Str ret {pte->avl, false};

    pte->avl |= flag;

    return ret;
}


kresult_t kernel_get_page(u64 virtual_addr, Page_Table_Argumments arg)
{
    void* r = kernel_pframe_allocator.alloc_page();

    try {
        kresult_t b = map((u64)r, virtual_addr, arg);
        if (b != SUCCESS) 
            kernel_pframe_allocator.free(r);
        
        return b;
    } catch (...) {
        kernel_pframe_allocator.free(r);
        throw;
    }
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
    if (not pte.present) return ERROR_PAGE_NOT_PRESENT;

    // Everything OK
    pte = PTE();
    invlpg(virtual_addr);

    return SUCCESS;
}


u64 x86_4level_Page_Table::phys_addr_of(u64 virt) const
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);

    // PML4 entry
    mapper.map((u64)pml4_phys); unsigned pml4_i = pml4_index(virt);
    if (not mapper.ptr[pml4_i].present)
        throw (Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "phys_addr_of pml4e not allocated"));

    // PDPT entry
    mapper.map(mapper.ptr[pml4_i].page_ppn << 12); unsigned pdpt_i = pdpt_index(virt);
    if (not mapper.ptr[pdpt_i].present)
        throw (Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "phys_addr_of pdpte not allocated"));

    // PD entry
    mapper.map(mapper.ptr[pdpt_i].page_ppn << 12); unsigned pd_i = pd_index(virt);
    if (not mapper.ptr[pd_i].present)
        throw (Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "phys_addr_of pde not allocated"));

    // PT entry
    mapper.map(mapper.ptr[pd_i].page_ppn << 12); unsigned pt_i = pt_index(virt);
    if (not mapper.ptr[pt_i].present)
        throw (Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "phys_addr_of pte not allocated"));

    return (mapper.ptr[pt_i].page_ppn << 12) | (virt & (u64)0xfff);
}

x86_4level_Page_Table::Page_Info x86_4level_Page_Table::get_page_mapping(u64 virt_addr) const
{
    Page_Info i{};

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);

    // PML4 entry
    mapper.map((u64)pml4_phys); unsigned pml4_i = pml4_index(virt_addr);
    if (not mapper.ptr[pml4_i].present)
        return i;

    // PDPT entry
    mapper.map(mapper.ptr[pml4_i].page_ppn << 12); unsigned pdpt_i = pdpt_index(virt_addr);
    if (not mapper.ptr[pdpt_i].present)
        return i;

    // PD entry
    mapper.map(mapper.ptr[pdpt_i].page_ppn << 12); unsigned pd_i = pd_index(virt_addr);
    if (not mapper.ptr[pd_i].present)
        return i;

    // PT entry
    mapper.map(mapper.ptr[pd_i].page_ppn << 12); unsigned pt_i = pt_index(virt_addr);
    i.flags = mapper.ptr[pt_i].avl;
    i.is_allocated = mapper.ptr[pt_i].present;
    i.dirty = mapper.ptr[pt_i].dirty;
    i.user_access = mapper.ptr[pt_i].user_access;
    i.page_addr = mapper.ptr[pt_i].page_ppn << 12;
    return i;
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

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::init_from_phys(u64 cr3)
{
    klib::shared_ptr<x86_4level_Page_Table> t = klib::unique_ptr<x86_4level_Page_Table>(new x86_4level_Page_Table());
    t->pml4_phys = (x86_PAE_Entry *)cr3;
    t->insert_global_page_tables();

    return t;
}

klib::shared_ptr<Page_Table> x86_4level_Page_Table::create_empty()
{
    klib::shared_ptr<x86_4level_Page_Table> new_table = klib::unique_ptr<x86_4level_Page_Table>(new x86_4level_Page_Table());


    // Get a free page
    void* l = nullptr; 
    try {
        l = kernel_pframe_allocator.alloc_page();
        u64 p = (u64)l;

        // Map pages
        Temp_Mapper_Obj<x86_PAE_Entry> new_page_m(get_cpu_struct()->temp_mapper);
        Temp_Mapper_Obj<x86_PAE_Entry> current_page_m(get_cpu_struct()->temp_mapper);

        new_page_m.map(p);
        current_page_m.map(getCR3());

        // Clear it as memory contains rubbish and it will cause weird paging bugs on real machines
        page_clear((void*)new_page_m.ptr);

        // Copy the last entries into the new page table as they are shared across all processes
        // and recurvicely assign the last page to itself
        // ((PML4*)free_page)->entries[509] = pml4(rec_map_index)->entries[509];
        new_page_m.ptr[510] = current_page_m.ptr[510];
        new_page_m.ptr[511] = current_page_m.ptr[511];

        new_page_m.ptr[rec_map_index] = x86_PAE_Entry();
        new_page_m.ptr[rec_map_index].present = 1;
        new_page_m.ptr[rec_map_index].user_access = 0;
        new_page_m.ptr[rec_map_index].page_ppn = p/KB(4);

        new_table->pml4_phys = (x86_PAE_Entry *)p;

        new_table->insert_global_page_tables();
    } catch (...) {
        if (l != nullptr)
            kernel_pframe_allocator.free(l);

        throw;
    }

    return new_table;
}

Page_Table::~Page_Table()
{
    paging_regions.clear();
    takeout_global_page_tables();
}

x86_4level_Page_Table::~x86_4level_Page_Table()
{
    free_user_pages();
    kernel_pframe_allocator.free((void*)pml4_phys);
}

void x86_4level_Page_Table::map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg)
{
    if (physical_addr >> 48)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "x86_4level_Page_Table::map physical page out of range"));

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);
    mapper.map((u64)pml4_phys);

    x86_PAE_Entry* pml4e = &mapper.ptr[pml4_index(virtual_addr)];
    if (not pml4e->present) {
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        *pml4e = x86_PAE_Entry(); pml4e->page_ppn = p; pml4e->present = 1; pml4e->writeable = 1; pml4e->user_access = arg.user_access;
        clear_page(p << 12);
    }

    mapper.map(pml4e->page_ppn << 12);
    x86_PAE_Entry* pdpte = &mapper.ptr[pdpt_index(virtual_addr)];
    if (pdpte->pat_size) // 1GB page is already present
        throw (Kern_Exception(ERROR_PAGE_PRESENT, "map 1G page is present"));
    
    if (not pdpte->present) {
        u64 p =  kernel_pframe_allocator.alloc_page_ppn();

        *pdpte = x86_PAE_Entry(); pdpte->page_ppn = p; pdpte->present = 1; pdpte->writeable = 1; pdpte->user_access = arg.user_access;
        clear_page(p << 12);
    }

    mapper.map(pdpte->page_ppn << 12);
    x86_PAE_Entry* pde = &mapper.ptr[pd_index(virtual_addr)];
    if (pde->pat_size)
        throw (Kern_Exception(ERROR_PAGE_PRESENT, "map 2M page is present"));
    
    if (not pde->present) {
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        *pde = x86_PAE_Entry(); pde->page_ppn = p; pde->present = 1; pde->writeable = 1; pde->user_access = arg.user_access;
        clear_page(p << 12);
    }

    mapper.map(pde->page_ppn << 12);
    x86_PAE_Entry* pte = &mapper.ptr[pt_index(virtual_addr)];
    if (pte->present)
        throw (Kern_Exception(ERROR_PAGE_PRESENT, "map page is present"));

    *pte = x86_PAE_Entry(); pte->page_ppn = physical_addr/KB(4); pte->present = 1; pte->user_access = arg.user_access; pte->writeable = arg.writeable;  
    pte->avl = arg.extra;
    if (nx_bit_enabled)
        pte->execution_disabled = arg.execution_disabled;
}

void x86_4level_Page_Table::free_pt(u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);
    mapper.map(pt_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        p->clear_auto();
    }
}

void x86_4level_Page_Table::free_pd(u64 pd_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);
    mapper.map(pd_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        if (p->present) {
            if (not p->pat_size) // Not a huge page
                free_pt(p->page_ppn << 12);

            p->clear_auto();
        }
    }
}

void x86_4level_Page_Table::free_pdpt(u64 pdpt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);
    mapper.map(pdpt_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        if (p->present) {
            if (not p->pat_size) // Not a huge page
                free_pt(p->page_ppn << 12);

            p->clear_auto();
        }
    }
}

void x86_4level_Page_Table::free_user_pages()
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(get_cpu_struct()->temp_mapper);
    mapper.map((u64)pml4_phys);

    for (u64 i = 0; i < 256; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        if (p->present) {
            free_pdpt(p->page_ppn << 12);
            kernel_pframe_allocator.free((void*)(p->page_ppn << 12));
        }
    }
}

u64 Page_Table::atomic_create_normal_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 pattern)
{
    Auto_Lock_Scope scope_lock(lock);

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    paging_regions.insert({start_addr,
        klib::make_shared<Private_Normal_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), weak_from_this(), access, pattern
        )});

    return start_addr;
}

u64 Page_Table::atomic_transfer_region(const klib::shared_ptr<Page_Table>& to, u64 region_orig, u64 prefered_to, u64 access, bool fixed)
{
    Auto_Lock_Scope_Double scope_lock(lock, to->lock);

    try {
        auto& reg = paging_regions.at(region_orig);

        if (prefered_to%07777)
            prefered_to = 0;

        u64 start_addr = to->find_region_spot(prefered_to, reg->size, fixed);
        reg->move_to(to, start_addr, access);
        return start_addr;
    } catch (std::out_of_range& r) {
        throw Kern_Exception(ERROR_OUT_OF_RANGE, "atomic_transfer_region source not found");
    }
}

u64 Page_Table::atomic_create_managed_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, klib::shared_ptr<Port> t)
{
    Auto_Lock_Scope scope_lock(lock);

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    paging_regions.insert({start_addr,
        klib::make_shared<Private_Managed_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), weak_from_this(), access, klib::forward<klib::shared_ptr<Port>>(t)
        )});

    return start_addr;
}

u64 Page_Table::atomic_create_phys_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 phys_addr_start)
{
    Auto_Lock_Scope scope_lock(lock);

    if (phys_addr_start >= phys_addr_limit() or phys_addr_start + page_aligned_size >= phys_addr_limit() or phys_addr_start > phys_addr_start+page_aligned_size)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "atomic_create_phys_region phys_addr outside the supported by x86"));

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    paging_regions.insert({start_addr,
        klib::make_shared<Phys_Mapped_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), weak_from_this(), access, phys_addr_start
        )});

    return start_addr;
}

Page_Table::pagind_regions_map::iterator Page_Table::get_region(u64 page)
{
    auto it = paging_regions.get_smaller_or_equal(page);
    if (it == paging_regions.end() or not it->second->is_in_range(page))
        return paging_regions.end();

    return it;
}

bool Page_Table::can_takeout_page(u64 page_addr) noexcept
{
    auto it = paging_regions.get_smaller_or_equal(page_addr);
    if (it == paging_regions.end() or not it->second->is_in_range(page_addr))
        return false;

    return it->second->can_takeout_page();
}

void Page_Table::provide_managed(u64 page_addr, u64 virt_addr)
{
    auto it = get_region(virt_addr);
    if (it == paging_regions.end())
        throw(Kern_Exception(ERROR_NO_FREE_REGION, "provide_managed no region found"));

    if (not it->second->is_managed())
        throw(Kern_Exception(ERROR_NOT_MANAGED_REGION, "provide_managed not a managed region"));

    Page_Table_Argumments args = it->second->craft_arguments();

    map(page_addr, virt_addr, args);
}

u64 Page_Table::find_region_spot(u64 desired_start, u64 size, bool fixed)
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

    // If the new region doesn't overlap with anything, just use it
    if (region_ok) {
        return desired_start;

    } else {
        if (fixed)
            throw(Kern_Exception(ERROR_REGION_OCCUPIED, "find_region_spot fixed region overlaps"));

        u64 addr = 0x1000;
        auto it = paging_regions.begin();
        while (it != paging_regions.end()) {
            u64 end = addr + size;

            if (it->first > end and end <= user_addr_max())
                return addr;

            addr = it->second->addr_end();
            ++it;
        }

        if (addr + size > user_addr_max())
            throw(Kern_Exception(ERROR_NO_FREE_REGION, "find_region_spot no free region found"));

        return addr;
    }
}

bool Page_Table::prepare_user_page(u64 virt_addr, unsigned access_type, const klib::shared_ptr<TaskDescriptor>& task)
{
    auto it = paging_regions.get_smaller_or_equal(virt_addr);

    if (it == paging_regions.end() or not it->second->is_in_range(virt_addr))
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "user provided parameter is unallocated"));

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

bool Page_Table::atomic_provide_page(const klib::shared_ptr<TaskDescriptor>& from_task, const klib::shared_ptr<Page_Table>& to, u64 page_from, u64 page_to, u64 flags)
{
    const klib::shared_ptr<Page_Table>& from = from_task->page_table;
    Auto_Lock_Scope_Double scope_lock(from->lock, to->lock);

    bool can_takeout_page = from->can_takeout_page(page_from);
    if (not can_takeout_page)
        throw (Kern_Exception(ERROR_NOT_SUPPORTED, "taking out the page from unsupported region"));

    // User page is not ready if not r
    bool r = from->prepare_user_page(page_from, Readable, from_task);
    if (not r)
        return r;

    u64 page = from->get_page_frame(page_from);

    to->provide_managed(page, page_to);

    to->unblock_tasks(page_to);

    from->invalidate(page_from, false);

    return r;
}

void Page_Table::unblock_tasks(u64 page)
{
    for (const auto& it : owner_tasks)
        it.lock()->atomic_try_unblock_by_page(page);
}

klib::shared_ptr<Page_Table> Page_Table::get_page_table(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    return global_page_tables.get_copy_or_default(id).lock();
}

klib::shared_ptr<Page_Table> Page_Table::get_page_table_throw(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    try {
        return global_page_tables.at(id).lock();
    } catch (const std::out_of_range&) {
        throw Kern_Exception(ERROR_OBJECT_DOESNT_EXIST, "requested page table doesn't exist");
    }
}

void Page_Table::map(u64 page_addr, u64 virt_addr)
{
    auto it = get_region(virt_addr);
    if (it == paging_regions.end())
        throw(Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "map no region found"));

    map(page_addr, virt_addr, it->second->craft_arguments());
}

u64 Page_Table::phys_addr_limit()
{
    return 0x01UL << 48;
}

void Page_Table::move_pages(const klib::shared_ptr<Page_Table>& to, u64 from_addr, u64 to_addr, u64 size_bytes, u8 access)
{
    u64 offset = 0;

    try {
        for (; offset < size_bytes; offset += 4096) {
            auto info = get_page_mapping(from_addr + offset);
            if (info.is_allocated) {
                Page_Table_Argumments arg = {
                    access & Writeable,
                    info.user_access,
                    0,
                    not (access & Executable),
                    info.flags,
                };
                to->map(info.page_addr, to_addr + offset, arg);
            }
        }

        invalidate_range(from_addr, size_bytes, false);
    } catch (...) {
        to->invalidate_range(to_addr, size_bytes, false);

        throw;
    }
}

void x86_4level_Page_Table::invalidate_range(u64 virt_addr, u64 size_bytes, bool free)
{
    // -------- CAN BE MADE MUCH FASTER ------------
    u64 end = virt_addr + size_bytes;
    for (u64 i = virt_addr; i < end; i += 4096)
        invalidate(i, free);
}

void x86_PAE_Entry::clear_nofree()
{
    *this = {};
}

void x86_PAE_Entry::clear_auto()
{
    if (present and not (avl & PAGING_FLAG_NOFREE))
            kernel_pframe_allocator.free((void*)(page_ppn << 12));

    *this = x86_PAE_Entry();
}