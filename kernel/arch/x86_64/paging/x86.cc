#include "x86.hh"

bool nx_bit_enabled = false;

kresult_t map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg, u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    u64 addr = virtual_addr;
    addr >>= 12;
    u64 ptable_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdir_entry = addr & 0x1ff;
    addr >>= 9;
    u64 pdpt_entry = addr & 0x1ff;

    u64 pml4_entry = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4 = mapper.map(pt_phys);
    x86_PAE_Entry& pml4e = pml4[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pml4e.page_ppn = p;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        clear_page(p << 12);
    }

    x86_PAE_Entry *pdpt = mapper.map(pml4e.page_ppn << 12);
    x86_PAE_Entry& pdpte = pdpt[pdpt_entry];
    if (pdpte.pat_size) return ERROR_PAGE_PRESENT;
    if (not pdpte.present) {
        pdpte = {};
        u64 p =  kernel_pframe_allocator.alloc_page_ppn();;

        pdpte.page_ppn = p;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        clear_page(p << 12);
    }

    x86_PAE_Entry *pd = mapper.map(pdpte.page_ppn << 12);
    x86_PAE_Entry& pde = pd[pdir_entry];
    if (pde.pat_size) return ERROR_PAGE_PRESENT;
    if (not pde.present) {
        pde = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pde.page_ppn = p;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        clear_page(p << 12);
    }

    x86_PAE_Entry *pt = mapper.map(pde.page_ppn << 12);
    x86_PAE_Entry& pte = pt[ptable_entry];
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

u64 get_pt_ppn(u64 virtual_addr, u64 pt_phys) {
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    u64 pml4_entry = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4 = mapper.map(pt_phys);
    x86_PAE_Entry& pml4e = pml4[pml4_entry];

    x86_PAE_Entry *pdpt = mapper.map(pml4e.page_ppn << 12);
    u64 pdpt_entry = (virtual_addr >> 30) & 0x1ff;
    x86_PAE_Entry& pdpte = pdpt[pdpt_entry];

    x86_PAE_Entry *pd = mapper.map(pdpte.page_ppn << 12);
    u64 pd_entry = (virtual_addr >> 21) & 0x1ff;
    x86_PAE_Entry& pde = pd[pd_entry];

    return pde.page_ppn;
}

u64 map_pages(u64 phys_addr, u64 virt_addr, u64 size_bytes, Page_Table_Argumments args, u64 cr3) {
    u64 result = SUCCESS;
    u64 i = 0;
    for (; i < size_bytes; i += 0x1000) {
        result = map(phys_addr + i, virt_addr + i, args, cr3);
        if (result != SUCCESS)
            break;
    }

    // TODO: It would probably be better to unmap the pages on error
    // Doesn't matter now
    // if (result != SUCCESS)
    //     ...

    return result;
}

u64 prepare_pt_for(u64 virtual_addr, Page_Table_Argumments arg, u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    u64 pml4_entry = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4 = mapper.map(pt_phys);
    x86_PAE_Entry& pml4e = pml4[pml4_entry];
    if (not pml4e.present) {
        pml4e = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pml4e.page_ppn = p;
        pml4e.present = 1;
        pml4e.writeable = 1;
        pml4e.user_access = arg.user_access;
        clear_page(p << 12);
    }

    x86_PAE_Entry *pdpt = mapper.map(pml4e.page_ppn << 12);
    u64 pdpt_entry = (virtual_addr >> 30) & 0x1ff;
    x86_PAE_Entry& pdpte = pdpt[pdpt_entry];
    if (pdpte.pat_size) return -1;
    if (not pdpte.present) {
        pdpte = {};
        u64 p =  kernel_pframe_allocator.alloc_page_ppn();

        pdpte.page_ppn = p;
        pdpte.present = 1;
        pdpte.writeable = 1;
        pdpte.user_access = arg.user_access;
        clear_page(p << 12);
    }

    x86_PAE_Entry *pd = mapper.map(pdpte.page_ppn << 12);
    u64 pd_entry = (virtual_addr >> 21) & 0x1ff;
    x86_PAE_Entry& pde = pd[pd_entry];
    if (pde.pat_size) return -1;
    if (not pde.present) {
        pde = {};
        u64 p = kernel_pframe_allocator.alloc_page_ppn();

        pde.page_ppn = p;
        pde.present = 1;
        pde.writeable = 1;
        pde.user_access = arg.user_access;
        clear_page(p << 12);
    }

    return pde.page_ppn << 12;
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

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

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
    
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

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
        invalidate_tlb(virt_addr, 4096);
}

bool x86_Page_Table::is_used_by_others() const
{
    return (active_count - is_active()) != 0;
}

bool x86_Page_Table::is_active() const
{
    return ::getCR3() == get_cr3();
}

void x86_Page_Table::invalidate_tlb(u64 page, u64 size)
{
    if (is_active())
        for (u64 i = 0; i < size; i += 4096)
            invlpg(page+i);

    if (is_used_by_others())
        ::signal_tlb_shootdown();
}

void x86_Page_Table::atomic_active_sum(u64 val) noexcept
{
    __atomic_add_fetch(&active_count, val, 0);
}

Page_Table::Check_Return_Str x86_4level_Page_Table::check_if_allocated_and_set_flag(u64 virtual_addr, u8 flag, Page_Table_Argumments arg)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

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
    u64 cr3 = getCR3();

    try {
        kresult_t b = map((u64)r, virtual_addr, arg, cr3);
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
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

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

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

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
    i.nofree = mapper.ptr[pt_i].avl & PAGING_FLAG_NOFREE;
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

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::create_empty()
{
    klib::shared_ptr<x86_4level_Page_Table> new_table = klib::unique_ptr<x86_4level_Page_Table>(new x86_4level_Page_Table());


    // Get a free page
    void* l = nullptr; 
    try {
        l = kernel_pframe_allocator.alloc_page();
        u64 p = (u64)l;

        // Map pages
        Temp_Mapper_Obj<x86_PAE_Entry> new_page_m(request_temp_mapper());
        Temp_Mapper_Obj<x86_PAE_Entry> current_page_m(request_temp_mapper());

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

x86_4level_Page_Table::~x86_4level_Page_Table()
{
    free_user_pages();
    kernel_pframe_allocator.free((void*)pml4_phys);
}

void x86_4level_Page_Table::map(u64 physical_addr, u64 virtual_addr, Page_Table_Argumments arg)
{
    if (physical_addr >> 48)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "x86_4level_Page_Table::map physical page out of range"));

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
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

void x86_4level_Page_Table::map(Page_Descriptor page, u64 virtual_addr, Page_Table_Argumments arg)
{
    if (page.page_ptr >> 48)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "x86_4level_Page_Table::map physical page out of range"));

    assert(page.available && "Page is not available");

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
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

    *pte = x86_PAE_Entry();
    pte->present = 1;
    pte->user_access = arg.user_access;
    pte->writeable = arg.writeable;  
    pte->avl = page.owning ? 0 : PAGING_FLAG_NOFREE;
    u64 page_ppn = page.takeout_page().first >> 12;
    pte->page_ppn = page_ppn;

    if (nx_bit_enabled)
        pte->execution_disabled = arg.execution_disabled;
}

void x86_4level_Page_Table::free_pt(u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(pt_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        p->clear_auto();
    }
}

void x86_4level_Page_Table::free_pd(u64 pd_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
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
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
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
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map((u64)pml4_phys);

    for (u64 i = 0; i < 256; ++i) {
        x86_PAE_Entry* p = &mapper.ptr[i];
        if (p->present) {
            free_pdpt(p->page_ppn << 12);
            kernel_pframe_allocator.free((void*)(p->page_ppn << 12));
        }
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

klib::shared_ptr<Page_Table> x86_4level_Page_Table::create_clone()
{
    klib::shared_ptr<x86_4level_Page_Table> new_table = create_empty();

    Auto_Lock_Scope_Double(this->lock, new_table->lock);

    if (new_table->mem_objects.size() != 0)
        // Somebody has messed with the page table while it was being created
        // I don't know if its the best solution to not block the tables immediately
        // but I believe it's better to block them for shorter time and abort the operation
        // when someone tries to mess with the paging, which would either be very poor
        // coding or a bug anyways
        throw Kern_Exception(ERROR_NAME_EXISTS, "page table is already cloned");

    try {
        for (auto &reg : this->mem_objects) {
            new_table->mem_objects.insert(
                    {reg.first, {
                        .max_privilege_mask = reg.second.max_privilege_mask,
                        .regions = {},}}
                        );
            
            Auto_Lock_Scope reg_lock(reg.first->pinned_lock);
            reg.first->register_pined(new_table->weak_from_this());
        }

        for (auto &reg : this->paging_regions) {
            reg.second->clone_to(new_table, reg.first, reg.second->access_type);
        }
    } catch (...) {
        // Remove all the regions and objects. It might not be necessary, since it should be handled by the destructor
        // but in case somebody from userspace specultively does weird stuff with the not-yet-fully-constructed page table, it's better
        // to give them an empty table
        for (const auto& reg : new_table->mem_objects)
            reg.first->atomic_unregister_pined(new_table->weak_from_this());

        auto it = new_table->paging_regions.begin();
        while (it != new_table->paging_regions.end()) {
            auto region_start = it->first;
            auto region_size = it->second->size;

            it->second->prepare_deletion();
            paging_regions.erase(region_start);

            invalidate_range(region_start, region_size, true);

            it = new_table->paging_regions.begin();
        }

        throw;
    }

    return new_table;  
}

