#include "x86_paging.hh"

#include <memory/mem_object.hh>
#include <memory/pmm.hh>
#include <x86_asm.hh>

IA32_Page_Table::page_table_map IA32_Page_Table::global_page_tables;
Spinlock IA32_Page_Table::page_table_index_lock;

using namespace kernel;

void apply_page_table(ptable_top_ptr_t page_table) { setCR3(page_table); }

void IA32_Page_Table::apply() { apply_page_table(cr3); }

bool use_pae    = false;
bool support_nx = false;

kresult_t ia32_map_page(u32 cr3, u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    assert(!(phys_addr & 0xFFF));
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto &pd_entry = active_pt[pd_idx];
        if (!(pd_entry & PAGE_PRESENT)) {
            auto new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return -ENOMEM;

            clear_page(new_pt_phys);

            u32 new_pt = new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            pd_entry   = new_pt;
        }

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        u32 *pt = pt_mapper.map(pd_entry & 0xFFFFF000);

        auto &pt_entry = pt[pt_idx];
        if (pt_entry & PAGE_PRESENT)
            return -EEXIST;

        u32 new_pt = phys_addr;
        new_pt |= PAGE_PRESENT;
        if (arg.writeable)
            new_pt |= PAGE_WRITE;
        if (arg.user_access)
            new_pt |= PAGE_USER;
        if (arg.global)
            new_pt |= PAGE_GLOBAL;
        new_pt |= avl_to_bits(arg.extra);
        if (arg.cache_policy != Memory_Type::Normal)
            new_pt |= PAGE_CD;

        pt_entry = new_pt;
        return 0;
    } else {
        // TODO
        return -ENOSYS;
    }
}

IA32_Page_Table::Page_Info IA32_Page_Table::get_page_mapping(u64 virt_addr) const
{
    Page_Info i {};
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto pd_entry = active_pt[pd_idx];
        if (!(pd_entry & PAGE_PRESENT)) {
            return i;
        }

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        u32 *pt = mapper.map(pd_entry & ~0xfff);

        u32 pte        = pt[pt_idx];
        i.flags        = avl_from_page(pte);
        i.is_allocated = !!(pte & PAGE_PRESENT);
        i.writeable    = !!(pte & PAGE_WRITE);
        i.executable   = 1;
        i.dirty        = !!(pte & PAGE_DIRTY);
        i.user_access  = !!(pte & PAGE_USER);
        i.page_addr    = pte & ~0xfff;
        i.nofree       = !!(i.flags & PAGING_FLAG_NOFREE);
    } else {
        assert(!"not implemented");
    }
    return i;
}

bool IA32_Page_Table::is_mapped(u64 virt_addr) const
{
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto pd_entry = active_pt[pd_idx];
        if (!(pd_entry & PAGE_PRESENT)) {
            return false;
        }

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        u32 *pt = mapper.map(pd_entry & ~0xfff);

        return pt[pt_idx] & PAGE_PRESENT;
    } else {
        Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
        u64 *active_pdpt = mapper.map(cr3);

        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;
        auto pt_idx   = (u32(virt_addr) >> 12) & 0x1FF;

        auto pdpt_entry = active_pdpt[pdpt_idx];
        if (!(pdpt_entry & PAGE_PRESENT))
            return false;

        Temp_Mapper_Obj<u64> pd_mapper(request_temp_mapper());
        u64 *pd = mapper.map(pdpt_entry & PAE_ADDR_MASK);

        auto pde = pd[pd_idx];
        if (!(pde & PAGE_PRESENT))
            return false;

        Temp_Mapper_Obj<u64> pt_mapper(request_temp_mapper());
        u64 *pt = mapper.map(pde & PAE_ADDR_MASK);

        return pt[pt_idx] & PAGE_PRESENT;
    }
}

kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    assert(!arg.user_access);
    const u32 cr3 = getCR3();
    return ia32_map_page(cr3, phys_addr, virt_addr, arg);
}

kresult_t IA32_Page_Table::map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg)
{
    return ia32_map_page(cr3, page_addr, (void *)virt_addr, arg);
}

kresult_t IA32_Page_Table::map(pmm::Page_Descriptor page, u64 virt_addr, Page_Table_Argumments arg)
{
    auto page_phys = page.get_phys_addr();
    arg.extra      = PAGING_FLAG_STRUCT_PAGE;
    auto result    = ia32_map_page(cr3, page_phys, (void *)virt_addr, arg);
    if (result == 0)
        page.takeout_page();
    return result;
}

kresult_t map_pages(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, u64 size,
                    Page_Table_Argumments arg)
{
    for (u64 i = 0; i < size; i += 4096) {
        auto result = ia32_map_page(page_table, phys_addr + i, (void *)(virt_addr + i), arg);
        if (result)
            return result;
    }
    return 0;
}

u64 prepare_pt_for(void *virt_addr, Page_Table_Argumments, u32 cr3)
{
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        if (!(active_pt[pd_idx] & PAGE_PRESENT)) {
            auto new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return -1ULL;

            clear_page(new_pt_phys);

            u32 new_pt        = new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            active_pt[pd_idx] = new_pt;
        }

        return active_pt[pd_idx] & 0xFFFFF000;
    } else {
        return -1ULL;
    }
}

void free_32bit_page(u32 entry)
{
    assert(entry & PAGE_PRESENT);

    auto avl = avl_from_page(entry);
    if (avl & PAGING_FLAG_STRUCT_PAGE) {
        auto p = pmm::Page_Descriptor::find_page_struct(entry & ~0xfff);
        assert(p.page_struct_ptr && "page struct must be present");
        p.release_taken_out_page();
    } else if (not(avl & PAGING_FLAG_NOFREE)) {
        pmm::free_memory_for_kernel(entry & ~0xfff, 1);
    }
}

void x86_invalidate_page(TLBShootdownContext &ctx, void *virt_addr, bool free, u32 cr3)
{
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto &pd_entry = active_pt[pd_idx];
        if (!(pd_entry & PAGE_PRESENT))
            return;

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        u32 *pt = pt_mapper.map(pd_entry & 0xFFFFF000);

        auto &pt_entry = pt[pt_idx];
        if (!(pt_entry & PAGE_PRESENT))
            return;

        u32 entry_saved = pt_entry;
        pt_entry        = 0;
        ctx.invalidate_page((u32)virt_addr);
        if (free)
            free_32bit_page(entry_saved);
    } else {
        // TODO
        assert(false);
    }
}

kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr)
{
    const u64 cr3 = getCR3();
    x86_invalidate_page(ctx, virt_addr, false, cr3);
    return 0;
}

void invalidate_tlb_kernel(u64 addr) { invlpg((void *)addr); }
void invalidate_tlb_kernel(u64 addr, u64 size)
{
    for (u64 i = 0; i < size; i += 4096)
        invlpg((void *)(addr + i));
}

klib::shared_ptr<IA32_Page_Table> IA32_Page_Table::capture_initial(u32 cr3)
{
    klib::shared_ptr<IA32_Page_Table> new_table =
        klib::unique_ptr<IA32_Page_Table>(new IA32_Page_Table());

    if (!new_table)
        return nullptr;

    new_table->cr3 = cr3;
    auto result    = insert_global_page_tables(new_table);
    if (result)
        return nullptr;

    return new_table;
}

kresult_t IA32_Page_Table::insert_global_page_tables(klib::shared_ptr<IA32_Page_Table> table)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    auto ret = global_page_tables.insert_noexcept({table->id, table});
    if (ret.first == global_page_tables.end())
        return -ENOMEM;

    return 0;
}

void IA32_Page_Table::tlb_flush_all() { setCR3(getCR3()); }

void IA32_Page_Table::free_user_pages()
{
    if (!use_pae) {
        u32 pd_limit = (u32)user_addr_max() >> 22;

        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *pd = mapper.map(cr3);

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());

        for (unsigned i = 0; i < pd_limit; ++i) {
            auto pde = pd[i];
            if (!(pde & PAGE_PRESENT))
                continue;

            auto pt = pt_mapper.map(pde & ~0xfff);
            for (unsigned i = 0; i < 1024; ++i) {
                u32 pte = pt[i];
                if (!(pte & PAGE_PRESENT))
                    continue;

                free_32bit_page(pte);
            }
            pmm::free_memory_for_kernel(pde & _32BIT_ADDR_MASK, 1);
        }
    } else {
        assert(!"not implemented");
    }
}

kresult_t IA32_Page_Table::resolve_anonymous_page(u64 virt_addr, u64 access_type)
{
    assert(access_type & Writeable);

    u64 ptt = prepare_pt_for((void *)virt_addr, {}, cr3);
    if (ptt == -1ULL)
        return -ENOMEM;

    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *pt = mapper.map(ptt);
        assert(pt);

        unsigned pt_index = ((u32)virt_addr >> 12) & 0x3ff;
        auto pte          = pt[pt_index];

        assert((pte & PAGE_PRESENT) && "Page must be present");
        assert((avl_from_page(pte) & PAGING_FLAG_STRUCT_PAGE) && "page must have struct page");
        assert(!(pte & PAGE_WRITE) && "page must be read-only");

        auto page = pmm::Page_Descriptor::find_page_struct(pte & _32BIT_ADDR_MASK);
        assert(page.page_struct_ptr && "page struct not found");

        if (__atomic_load_n(&page.page_struct_ptr->l.refcount, __ATOMIC_ACQUIRE) == 2) {
            // only owner of the page
            pte |= PAGE_WRITE;
            pt[pt_index] = pte;
            return 0;
        }

        auto owner = page.page_struct_ptr->l.owner;
        assert(owner && "page owner not found");

        auto new_descriptor =
            owner->atomic_request_anonymous_page(page.page_struct_ptr->l.offset, true);
        if (!new_descriptor.success())
            return new_descriptor.result;

        pt[pt_index] = 0;

        {
            auto tlb_ctx = TLBShootdownContext::create_userspace(*this);
            tlb_ctx.invalidate_page(virt_addr & ~0xffful);
        }

        u64 new_page_phys = new_descriptor.val.takeout_page();
        assert(!(new_page_phys >> 32));

        Temp_Mapper_Obj<void> old_page(request_temp_mapper());
        old_page.map(pte & _32BIT_ADDR_MASK);
        Temp_Mapper_Obj<void> new_page(request_temp_mapper());
        new_page.map(new_page_phys);

        __builtin_memcpy(new_page.ptr, old_page.ptr, 4096);
        page.release_taken_out_page();

        pte &= ~_32BIT_ADDR_MASK;
        pte |= new_page_phys;
        pt[pt_index] = pte;
        return 0;
    } else {
        Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
        u64 *pt = mapper.map(ptt);
        assert(pt);

        unsigned pt_index = ((u32)virt_addr >> 12) & 0x1ff;
        auto pte          = pt[pt_index];

        assert((pte & PAGE_PRESENT) && "Page must be present");
        assert((avl_from_page(pte) & PAGING_FLAG_STRUCT_PAGE) && "page must have struct page");
        assert(!(pte & PAGE_WRITE) && "page must be read-only");

        auto page = pmm::Page_Descriptor::find_page_struct(pte & PAE_ADDR_MASK);
        assert(page.page_struct_ptr && "page struct not found");

        if (__atomic_load_n(&page.page_struct_ptr->l.refcount, __ATOMIC_ACQUIRE) == 2) {
            // only owner of the page
            pte |= PAGE_WRITE;
            pt[pt_index] = pte;
            return 0;
        }

        auto owner = page.page_struct_ptr->l.owner;
        assert(owner && "page owner not found");

        auto new_descriptor =
            owner->atomic_request_anonymous_page(page.page_struct_ptr->l.offset, true);
        if (!new_descriptor.success())
            return new_descriptor.result;

        pt[pt_index] = 0;

        {
            auto tlb_ctx = TLBShootdownContext::create_userspace(*this);
            tlb_ctx.invalidate_page(virt_addr & ~0xffful);
        }

        u64 new_page_phys = new_descriptor.val.takeout_page();

        Temp_Mapper_Obj<void> old_page(request_temp_mapper());
        old_page.map(pte & PAE_ADDR_MASK);
        Temp_Mapper_Obj<void> new_page(request_temp_mapper());
        new_page.map(new_page_phys);

        __builtin_memcpy(new_page.ptr, old_page.ptr, 4096);
        page.release_taken_out_page();

        pte &= ~PAE_ADDR_MASK;
        pte |= new_page_phys;
        pt[pt_index] = pte;
        return 0;
    }
}

IA32_Page_Table::~IA32_Page_Table()
{
    takeout_global_page_tables();

    if (cr3 != -1U) {
        free_user_pages();
        if (!use_pae) {
            pmm::free_memory_for_kernel(cr3, 1);
        } else {
            assert(!"not implemented");
        }
    }
}