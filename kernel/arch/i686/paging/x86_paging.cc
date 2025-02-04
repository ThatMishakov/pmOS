#include "x86_paging.hh"

#include <memory/mem_object.hh>
#include <memory/pmm.hh>
#include <pmos/utility/scope_guard.hh>
#include <x86_asm.hh>
#include <memory>

IA32_Page_Table::page_table_map IA32_Page_Table::global_page_tables;
Spinlock IA32_Page_Table::page_table_index_lock;

using namespace kernel;

void apply_page_table(ptable_top_ptr_t page_table) { setCR3(page_table); }

void IA32_Page_Table::apply() { apply_page_table(cr3); }

bool use_pae    = false;
bool support_nx = false;

extern u32 idle_cr3;

template<typename T> static T alignup(T input, unsigned alignment_log)
{
    T i = 1;
    i <<= alignment_log;
    T mask = i - 1;
    return (input + mask) & ~mask;
}

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
    return ia32_map_page(idle_cr3, phys_addr, virt_addr, arg);
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

static void x86_invalidate_pages(TLBShootdownContext &ctx, u32 cr3, void *virt_addr, size_t size,
                                 bool free)
{
    if (!use_pae) {
        u32 limit        = u32(virt_addr) + size;
        u32 first_pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        u32 end_aligned  = alignup((u32(virt_addr) + size), 22);
        u32 last_idx     = (end_aligned >> 22) & 0x3FF;
        if (last_idx == 0 && end_aligned != (u32)virt_addr)
            last_idx = 1024;

        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        for (u32 i = first_pd_idx; i < last_idx; ++i) {
            auto pd_entry = active_pt[i];
            if (!(pd_entry & PAGE_PRESENT))
                continue;

            Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
            u32 *pt = pt_mapper.map(pd_entry & _32BIT_ADDR_MASK);

            u32 addr_of_pd = i << 22;
            u32 start_idx  = addr_of_pd > (u32)virt_addr ? 0 : ((u32)virt_addr >> 12) & 0x3FF;
            u32 end_idx    = (addr_of_pd + 0x400000) == 0 || (addr_of_pd + 0x400000) < limit
                                 ? 1024
                                 : (limit >> 12) & 0x3FF;
            for (unsigned j = start_idx; j < end_idx; ++j) {
                auto pt_entry = pt[j];
                if (!(pt_entry & PAGE_PRESENT))
                    continue;

                u32 entry_saved = pt_entry;
                pt[j]           = 0;
                ctx.invalidate_page((addr_of_pd + (j << 12)));
                if (free)
                    free_32bit_page(entry_saved);
            }
        }
    } else {
        assert(!"not implemented");
    }
}

kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr)
{
    x86_invalidate_page(ctx, virt_addr, false, idle_cr3);
    return 0;
}

void IA32_Page_Table::invalidate(TLBShootdownContext &ctx, u64 virt_addr, bool free)
{
    x86_invalidate_page(ctx, (void *)virt_addr, free, cr3);
}

void IA32_Page_Table::invalidate_range(TLBShootdownContext &ctx, u64 virt_addr, u64 size_bytes,
                                       bool free)
{
    x86_invalidate_pages(ctx, cr3, (void *)virt_addr, size_bytes, free);
}

void invalidate_tlb_kernel(u64 addr) { invlpg((void *)addr); }
void invalidate_tlb_kernel(u64 addr, u64 size)
{
    for (u64 i = 0; i < size; i += 4096)
        invlpg((void *)(addr + i));
}

void IA32_Page_Table::invalidate_tlb(u64 addr) { invlpg((void *)addr); }
void IA32_Page_Table::invalidate_tlb(u64 page, u64 size)
{
    if (size / 4096 < 64)
        for (u64 i = 0; i < size; i += 4096)
            invlpg((void *)(page + i));

    else
        setCR3(getCR3());
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

kresult_t IA32_Page_Table::copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to,
                                                u64 from_addr, u64 to_addr, u64 size_bytes,
                                                u8 access)
{
    u32 offset = 0;

    auto guard = pmos::utility::make_scope_guard([&]() {
        auto ctx = TLBShootdownContext::create_userspace(*to);
        to->invalidate_range(ctx, to_addr, offset, true);
    });

    kresult_t result;
    {
        TLBShootdownContext ctx = TLBShootdownContext::create_userspace(*this);
        result                  = [&]() -> kresult_t {
            if (!use_pae) {
                u32 start_index     = (from_addr >> 22) & 0x3FF;
                u32 limit           = from_addr + size_bytes;
                u32 to_addr_aligned = alignup(limit, 22);
                u32 end_index       = (to_addr_aligned >> 22) & 0x3FF;

                Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
                auto pd = mapper.map(cr3);

                Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());

                for (unsigned i = start_index; i < end_index; ++i) {
                    auto pde = pd[i];
                    if (!(pde & PAGE_PRESENT))
                        continue;

                    auto pt = pt_mapper.map(pde & _32BIT_ADDR_MASK);

                    u32 addr_of_pd = (1 << 22) * i;
                    u32 start_idx  = addr_of_pd > from_addr ? 0 : (from_addr >> 12) & 0x3ff;
                    u32 end_of_pd  = (1 << 22) * (i + 1);
                    u32 end_idx    = end_of_pd <= limit ? 1024 : (limit >> 12) & 0x3ff;
                    for (unsigned j = start_idx; j < end_idx; ++j) {
                        auto pte = pt[j];

                        if (!(pte & PAGE_PRESENT))
                            continue;

                        auto pp = pmm::Page_Descriptor::find_page_struct(pte & _32BIT_ADDR_MASK);
                        assert(pp.page_struct_ptr && "page struct must be present");
                        if (!pp.page_struct_ptr->is_anonymous())
                            continue;

                        u32 copy_from = (1 << 22) * i + (1 << 12) * j;
                        u32 copy_to   = copy_from - from_addr + to_addr;

                        Page_Table_Argumments arg = {
                                             .readable           = !!(access & Readable),
                                             .writeable          = false,
                                             .user_access        = !!(pte & PAGE_USER),
                                             .global             = !!(pte & PAGE_GLOBAL),
                                             .execution_disabled = !(access & Executable),
                                             .extra              = avl_from_page(pte),
                                             .cache_policy =
                                pte & PAGE_CD ? Memory_Type::IONoCache : Memory_Type::Normal,
                        };

                        if (pte & PAGE_WRITE) {
                            pte &= ~PAGE_WRITE;
                            pt[j] = pte;
                            ctx.invalidate_page(copy_from);
                        }

                        auto result = to->map(klib::move(pp), copy_to, arg);
                        if (result)
                            return result;

                        offset = copy_from - from_addr + PAGE_SIZE;
                    }
                }
            } else {
                // TODO
                assert(false);
            }

            return 0;
        }();
    }

    if (result == 0)
        guard.dismiss();

    return result;
}

void IA32_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase(this->id);
}

klib::shared_ptr<IA32_Page_Table> IA32_Page_Table::get_page_table(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);
    auto it = global_page_tables.find(id);
    if (it == global_page_tables.end())
        return nullptr;
    return it->second.lock();
}

u64 IA32_Page_Table::user_addr_max() const { return 0xC0000000; }

klib::shared_ptr<IA32_Page_Table> IA32_Page_Table::create_empty(unsigned)
{
    klib::shared_ptr<IA32_Page_Table> new_table =
        klib::unique_ptr<IA32_Page_Table>(new IA32_Page_Table());

    if (!new_table)
        return nullptr;

    if (!use_pae) {
        auto cr3 = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(cr3))
            return nullptr;
        auto guard =
            pmos::utility::make_scope_guard([&]() { pmm::free_memory_for_kernel(cr3, 1); });

        Temp_Mapper_Obj<u32> new_page_m(request_temp_mapper());
        Temp_Mapper_Obj<u32> current_page_m(request_temp_mapper());

        auto new_page     = new_page_m.map(cr3);
        auto current_page = current_page_m.map(idle_cr3);

        // Clear new page
        for (size_t i = 0; i < 768; ++i)
            new_page[i] = 0;

        // Copy kernel entries
        for (size_t i = 768; i < 1024; ++i)
            new_page[i] = current_page[i];

        new_table->cr3 = cr3;

        auto r = insert_global_page_tables(new_table);
        if (r)
            return nullptr;

        guard.dismiss();

        return new_table;
    } else {
        assert(!"not implemented");
    }
    return nullptr;
}

klib::shared_ptr<IA32_Page_Table> IA32_Page_Table::create_clone()
{
    klib::shared_ptr<IA32_Page_Table> new_table = create_empty();
    if (!new_table)
        return nullptr;

    Auto_Lock_Scope_Double scope_guard(this->lock, new_table->lock);

    if (!new_table->mem_objects.empty() || !new_table->object_regions.empty() ||
        !new_table->paging_regions.empty())
        // Somebody has messed with the page table while it was being created
        // I don't know if it's the best solution to not block the tables
        // immediately but I believe it's better to block them for shorter time
        // and abort the operation when someone tries to mess with the paging,
        // which would either be very poor coding or a bug anyways
        return nullptr; // "page table is already cloned"

    // This gets called on error
    auto guard = pmos::utility::make_scope_guard([&]() {
        // Remove all the regions and objects. It might not be necessary, since
        // it should be handled by the destructor but in case somebody from
        // userspace specultively does weird stuff with the
        // not-yet-fully-constructed page table, it's better to give them an
        // empty table

        for (const auto &reg: new_table->mem_objects)
            reg.first->atomic_unregister_pined(new_table->weak_from_this());

        auto tlb_ctx = TLBShootdownContext::create_userspace(*new_table);

        auto it = new_table->paging_regions.begin();
        while (it != new_table->paging_regions.end()) {
            auto region_start = it->start_addr;
            auto region_size  = it->size;

            it->prepare_deletion();
            new_table->paging_regions.erase(it);

            new_table->invalidate_range(tlb_ctx, region_start, region_size, true);

            it = new_table->paging_regions.begin();
        }
    });

    for (auto &reg: this->mem_objects) {
        auto res =
            new_table->mem_objects.insert({reg.first,
                                           {
                                               .max_privilege_mask = reg.second.max_privilege_mask,
                                           }});
        if (res.first == new_table->mem_objects.end())
            return nullptr;

        Auto_Lock_Scope reg_lock(reg.first->pinned_lock);
        auto result = reg.first->register_pined(new_table->weak_from_this());
        if (result)
            return nullptr;
    }

    for (auto &reg: this->paging_regions) {
        auto result = reg.clone_to(new_table, reg.start_addr, reg.access_type);
        if (result)
            return nullptr;
    }

    guard.dismiss();

    return new_table;
}

bool page_mapped(void *pagefault_cr2, ulong err)
{
    bool write = err & 0x02;
    bool exec  = err & 0x10;

    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        auto pd = mapper.map(getCR3() & 0xFFFFF000);

        auto pd_idx = (u32(pagefault_cr2) >> 22) & 0x3FF;
        auto pt_idx = (u32(pagefault_cr2) >> 12) & 0x3FF;

        auto pde = pd[pd_idx];
        if (!(pde & PAGE_PRESENT))
            return false;

        if (write && !(pde & PAGE_WRITE))
            return false;

        Temp_Mapper_Obj<u32> pt_mapper(request_temp_mapper());
        auto pt = pt_mapper.map(pde & 0xFFFFF000);

        auto pte = pt[pt_idx];

        if (!(pte & PAGE_PRESENT))
            return false;

        if (write && !(pte & PAGE_WRITE))
            return false;

        return true;
    } else {
        Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
        auto pdpt = mapper.map(getCR3() & 0xFFFFFFE0);

        auto pdpt_idx = (u32(pagefault_cr2) >> 30) & 0x3;
        auto pd_idx   = (u32(pagefault_cr2) >> 21) & 0x1FF;
        auto pt_idx   = (u32(pagefault_cr2) >> 12) & 0x1FF;

        auto pdpt_entry = __atomic_load_n(std::assume_aligned<8>(pdpt + pdpt_idx), __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT))
            return false;

        if (write && !(pdpt_entry & PAGE_WRITE))
            return false;

        if (exec && (pdpt_entry & PAGE_NX))
            return false;

        Temp_Mapper_Obj<u64> pd_mapper(request_temp_mapper());
        auto pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        auto pde = __atomic_load_n(std::assume_aligned<8>(pd + pd_idx), __ATOMIC_RELAXED);
        if (!(pde & PAGE_PRESENT))
            return false;

        if (write && !(pde & PAGE_WRITE))
            return false;

        if (exec && (pde & PAGE_NX))
            return false;

        Temp_Mapper_Obj<u64> pt_mapper(request_temp_mapper());
        auto pt = pt_mapper.map(pde & PAE_ADDR_MASK);

        auto pte = __atomic_load_n(std::assume_aligned<8>(pt + pt_idx), __ATOMIC_RELAXED);
        if (!(pte & PAGE_PRESENT))
            return false;

        if (write && !(pte & PAGE_WRITE))
            return false;

        if (exec && (pte & PAGE_NX))
            return false;

        return true;
    }
}