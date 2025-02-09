#include "x86_paging.hh"

#include <memory/mem_object.hh>
#include <memory/pmm.hh>
#include <memory>
#include <pmos/utility/scope_guard.hh>
#include <utility>
#include <x86_asm.hh>
#include <x86_utils.hh>

IA32_Page_Table::page_table_map IA32_Page_Table::global_page_tables;
Spinlock IA32_Page_Table::page_table_index_lock;

using namespace kernel;

void IA32_Page_Table::apply() { apply_page_table(cr3); }

bool use_pae    = false;
bool support_nx = false;

bool detect_nx()
{
    if (!use_pae)
        return false;

    auto c = cpuid(0x80000001);
    support_nx = c.edx & (1 << 20);
    return support_nx;
}

void apply_page_table(ptable_top_ptr_t page_table) {
    if (support_nx) {
        const unsigned msr = 0xC0000080;
        write_msr(msr, read_msr(msr) | (1 << 11));
    }

    setCR3(page_table);
}

extern u32 idle_cr3;

static std::pair<u32, u32> cr3_pae_page_offset(u32 cr3)
{
    return {cr3 & 0xFFFFF000, (cr3 & 0xFE0) / sizeof(pae_entry_t)};
}

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
        // if (arg.cache_policy != Memory_Type::Normal)
        //     new_pt |= PAGE_CD;

        pt_entry = new_pt;
        return 0;
    } else {
        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;
        auto pt_idx   = (u32(virt_addr) >> 12) & 0x1FF;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys & 0xFFFFFFE0) + cr3_offset;

        pae_entry_t pdpt_entry = __atomic_load_n(pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT)) {
            auto new_pd_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pd_phys))
                return -ENOMEM;

            clear_page(new_pd_phys);

            pdpt_entry = new_pd_phys | PAGE_PRESENT;
            __atomic_store_n(pdpt + pdpt_idx, pdpt_entry, __ATOMIC_RELAXED);
        }

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pd_entry = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pd_entry & PAGE_PRESENT)) {
            auto new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return -ENOMEM;

            clear_page(new_pt_phys);

            pd_entry = new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            __atomic_store_n(pd + pd_idx, pd_entry, __ATOMIC_RELAXED);
        }

        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
        pae_entry_t *pt = pt_mapper.map(pd_entry & PAE_ADDR_MASK);

        pae_entry_t pt_entry = __atomic_load_n(pt + pt_idx, __ATOMIC_RELAXED);
        if (pt_entry & PAGE_PRESENT)
            return -EEXIST;

        pae_entry_t new_pt = phys_addr;
        new_pt |= PAGE_PRESENT;
        if (arg.writeable)
            new_pt |= PAGE_WRITE;
        if (arg.user_access)
            new_pt |= PAGE_USER;
        if (arg.global)
            new_pt |= PAGE_GLOBAL;
        new_pt |= avl_to_bits(arg.extra);
        if (support_nx && arg.execution_disabled)
            new_pt |= PAGE_NX;
        // if (arg.cache_policy != Memory_Type::Normal)
        //     new_pt |= PAGE_CD;

        __atomic_store_n(pt + pt_idx, new_pt, __ATOMIC_RELAXED);
        return 0;
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
        i.page_addr    = pte & _32BIT_ADDR_MASK;
        i.nofree       = !!(i.flags & PAGING_FLAG_NOFREE);
    } else {
        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;
        auto pt_idx   = (u32(virt_addr) >> 12) & 0x1FF;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        pae_entry_t pdpt_entry = __atomic_load_n(pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT)) {
            return i;
        }

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pd_entry = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pd_entry & PAGE_PRESENT)) {
            return i;
        }

        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
        pae_entry_t *pt = pt_mapper.map(pd_entry & PAE_ADDR_MASK);

        pae_entry_t pt_entry = __atomic_load_n(pt + pt_idx, __ATOMIC_RELAXED);
        i.flags              = avl_from_page(pt_entry);
        i.is_allocated       = !!(pt_entry & PAGE_PRESENT);
        i.writeable          = !!(pt_entry & PAGE_WRITE);
        i.executable         = 1;
        i.dirty              = !!(pt_entry & PAGE_DIRTY);
        i.user_access        = !!(pt_entry & PAGE_USER);
        i.page_addr          = pt_entry & PAE_ADDR_MASK;
        i.nofree             = !!(i.flags & PAGING_FLAG_NOFREE);
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
        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *active_pdpt = mapper.map(cr3_phys) + cr3_offset;

        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;
        auto pt_idx   = (u32(virt_addr) >> 12) & 0x1FF;

        pae_entry_t pdpt_entry = __atomic_load_n(active_pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT))
            return false;

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pde = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pde & PAGE_PRESENT))
            return false;

        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
        pae_entry_t *pt = mapper.map(pde & PAE_ADDR_MASK);

        return __atomic_load_n(pt + pt_idx, __ATOMIC_RELAXED) & PAGE_PRESENT;
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

        return active_pt[pd_idx] & _32BIT_ADDR_MASK;
    } else {
        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        pae_entry_t pdpt_entry = __atomic_load_n(pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT)) {
            auto new_pd_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pd_phys))
                return -1ULL;

            clear_page(new_pd_phys);

            pdpt_entry = new_pd_phys | PAGE_PRESENT;
            __atomic_store_n(pdpt + pdpt_idx, pdpt_entry, __ATOMIC_RELAXED);
        }

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pd_entry = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pd_entry & PAGE_PRESENT)) {
            auto new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return -1ULL;

            clear_page(new_pt_phys);

            pd_entry = new_pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            __atomic_store_n(pd + pd_idx, pd_entry, __ATOMIC_RELAXED);
        }

        assert(pd_entry & PAGE_PRESENT);

        return pd_entry & PAE_ADDR_MASK;
    }
}

void free_32bit_page(pae_entry_t entry)
{
    assert(entry & PAGE_PRESENT);

    auto avl = avl_from_page(entry);
    if (avl & PAGING_FLAG_STRUCT_PAGE) {
        auto p = pmm::Page_Descriptor::find_page_struct(entry & PAE_ADDR_MASK);
        assert(p.page_struct_ptr && "page struct must be present");
        p.release_taken_out_page();
    } else if (not(avl & PAGING_FLAG_NOFREE)) {
        pmm::free_memory_for_kernel(entry & PAE_ADDR_MASK, 1);
    }
}

void x86_invalidate_page(TLBShootdownContext &ctx, void *virt_addr, bool free, u32 cr3)
{
    if (!use_pae) {
        Temp_Mapper_Obj<u32> mapper(request_temp_mapper());
        u32 *active_pt = mapper.map(cr3);

        auto pd_idx = (u32(virt_addr) >> 22) & 0x3FF;
        auto pt_idx = (u32(virt_addr) >> 12) & 0x3FF;

        auto pd_entry = active_pt[pd_idx];
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
        auto pdpt_idx = (u32(virt_addr) >> 30) & 0x3;
        auto pd_idx   = (u32(virt_addr) >> 21) & 0x1FF;
        auto pt_idx   = (u32(virt_addr) >> 12) & 0x1FF;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        pae_entry_t pdpt_entry = __atomic_load_n(pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT))
            return;

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pd_entry = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pd_entry & PAGE_PRESENT))
            return;

        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
        pae_entry_t *pt = pt_mapper.map(pd_entry & PAE_ADDR_MASK);

        pae_entry_t pt_entry = __atomic_load_n(pt + pt_idx, __ATOMIC_RELAXED);
        if (!(pt_entry & PAGE_PRESENT))
            return;

        __atomic_store_n(pt + pt_idx, 0, __ATOMIC_RELEASE);
        ctx.invalidate_page((u32)virt_addr);
        if (free)
            free_32bit_page(pt_entry);
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
            u32 end_idx    = (limit == 0 && virt_addr != nullptr) or
                                  (last_idx == 1024 and i != 1023) or
                                  ((limit >= 0x400000) and (addr_of_pd >= limit - 0x400000))
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
        u32 limit       = u32(virt_addr) + size;
        u32 first_pdpt  = (u32(virt_addr) >> 30) & 0x3;
        u32 end_aligned = alignup((u32(virt_addr) + size), 30);
        u32 last_pdpt   = (end_aligned >> 30) & 0x3;
        if (last_pdpt == 0 && (u32)virt_addr != 0)
            last_pdpt = 4;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        for (u32 i = first_pdpt; i < last_pdpt; ++i) {
            auto pdpt_entry = __atomic_load_n(pdpt + i, __ATOMIC_RELAXED);
            if (!(pdpt_entry & PAGE_PRESENT))
                continue;

            Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
            pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

            u32 limit_end_aligned = alignup(limit, 21);
            u32 addr_of_pdpt = i << 30;
            u32 start_pd     = addr_of_pdpt > (u32)virt_addr ? 0 : ((u32)virt_addr >> 21) & 0x1FF;
            u32 end_pd;
            if (limit_end_aligned == 0 && virt_addr != nullptr)
                end_pd = 512;
            else if (last_pdpt == 4 && i != 3)
                end_pd = 512;
            else if ((limit_end_aligned >= 0x40000000) and addr_of_pdpt <= (limit_end_aligned - 0x40000000))
                end_pd = 512;
            else
                end_pd = (limit_end_aligned >> 21) & 0x1FF;

            for (unsigned j = start_pd; j < end_pd; ++j) {
                auto pd_entry = __atomic_load_n(pd + j, __ATOMIC_RELAXED);
                if (!(pd_entry & PAGE_PRESENT))
                    continue;

                Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
                pae_entry_t *pt = pt_mapper.map(pd_entry & PAE_ADDR_MASK);

                u32 addr_of_pd = addr_of_pdpt + (j << 21);
                u32 start_idx  = addr_of_pd > (u32)virt_addr ? 0 : ((u32)virt_addr >> 12) & 0x1FF;
                u32 end_idx;
                if (limit == 0 && virt_addr != nullptr)
                    end_idx = 512;
                else if (last_pdpt == 4 and i != 3)
                    end_idx = 512;
                else if ((limit >= 0x200000) and addr_of_pd <= (limit - 0x200000))
                    end_idx = 512;
                else
                    end_idx = (limit >> 12) & 0x1FF;

                for (unsigned k = start_idx; k < end_idx; ++k) {
                    auto pt_entry = __atomic_load_n(pt + k, __ATOMIC_RELAXED);
                    if (!(pt_entry & PAGE_PRESENT))
                        continue;

                    __atomic_store_n(pt + k, 0, __ATOMIC_RELEASE);
                    ctx.invalidate_page((addr_of_pd + (k << 12)));
                    if (free)
                        free_32bit_page(pt_entry);
                }
            }
        }
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
        u32 pdpt_limit = (u32)user_addr_max() >> 30;

        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());

        for (unsigned i = 0; i < pdpt_limit; ++i) {
            pae_entry_t pdpt_entry = __atomic_load_n(pdpt + i, __ATOMIC_RELAXED);
            if (!(pdpt_entry & PAGE_PRESENT))
                continue;

            pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);
            for (unsigned j = 0; j < 512; ++j) {
                pae_entry_t pde = __atomic_load_n(pd + j, __ATOMIC_RELAXED);
                if (!(pde & PAGE_PRESENT))
                    continue;

                pae_entry_t *pt = pt_mapper.map(pde & PAE_ADDR_MASK);
                for (unsigned k = 0; k < 512; ++k) {
                    pae_entry_t pte = __atomic_load_n(pt + k, __ATOMIC_RELAXED);
                    if (!(pte & PAGE_PRESENT))
                        continue;

                    free_32bit_page(pte);
                }
                pmm::free_memory_for_kernel(pde & PAE_ADDR_MASK, 1);
            }
            pmm::free_memory_for_kernel(pdpt_entry & PAE_ADDR_MASK, 1);
        }
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
        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pt = mapper.map(ptt);
        assert(pt);

        unsigned pt_index = ((u32)virt_addr >> 12) & 0x1ff;
        pae_entry_t pte   = __atomic_load_n(pt + pt_index, __ATOMIC_RELAXED);

        assert((pte & PAGE_PRESENT) && "Page must be present");
        assert((avl_from_page(pte) & PAGING_FLAG_STRUCT_PAGE) && "page must have struct page");
        assert(!(pte & PAGE_WRITE) && "page must be read-only");

        auto page = pmm::Page_Descriptor::find_page_struct(pte & PAE_ADDR_MASK);
        assert(page.page_struct_ptr && "page struct not found");

        if (__atomic_load_n(&page.page_struct_ptr->l.refcount, __ATOMIC_ACQUIRE) == 2) {
            // only owner of the page
            pte |= PAGE_WRITE;
            __atomic_store_n(pt + pt_index, pte, __ATOMIC_RELEASE);
            return 0;
        }

        auto owner = page.page_struct_ptr->l.owner;
        assert(owner && "page owner not found");

        auto new_descriptor =
            owner->atomic_request_anonymous_page(page.page_struct_ptr->l.offset, true);
        if (!new_descriptor.success())
            return new_descriptor.result;

        __atomic_store_n(pt + pt_index, 0, __ATOMIC_RELEASE);

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
        __atomic_store_n(pt + pt_index, pte, __ATOMIC_RELEASE);
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
            free_pae_cr3(cr3);
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
                u32 start_index     = (from_addr >> 30) & 0x3;
                u32 limit           = from_addr + size_bytes;
                u32 to_addr_aligned = alignup(limit, 30);
                u32 end_index       = (to_addr_aligned >> 30) & 0x3;

                auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

                Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
                pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

                Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
                Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());

                for (unsigned i = start_index; i < end_index; ++i) {
                    pae_entry_t pdpt_entry = __atomic_load_n(pdpt + i, __ATOMIC_RELAXED);
                    if (!(pdpt_entry & PAGE_PRESENT))
                        continue;

                    pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

                    u32 to_pd_aligned = alignup(limit, 21);
                    u32 addr_of_pdpt = (1 << 30) * i;
                    u32 start_pd     = addr_of_pdpt > from_addr ? 0 : (from_addr >> 21) & 0x1FF;
                    u32 end_of_pdpt  = (1 << 30) * (i + 1);
                    u32 end_pd       = end_of_pdpt <= to_pd_aligned ? 512 : (to_pd_aligned >> 21) & 0x1FF;
                    for (unsigned j = start_pd; j < end_pd; ++j) {
                        pae_entry_t pde = __atomic_load_n(pd + j, __ATOMIC_RELAXED);
                        if (!(pde & PAGE_PRESENT))
                            continue;

                        pae_entry_t *pt = pt_mapper.map(pde & PAE_ADDR_MASK);

                        u32 addr_of_pd = addr_of_pdpt + (1 << 21) * j;
                        u32 start_idx  = addr_of_pd > from_addr ? 0 : (from_addr >> 12) & 0x1FF;
                        u32 end_of_pd  = addr_of_pd + (1 << 21);
                        u32 end_idx    = end_of_pd <= limit ? 512 : (limit >> 12) & 0x1FF;
                        for (unsigned k = start_idx; k < end_idx; ++k) {
                            pae_entry_t pte = __atomic_load_n(pt + k, __ATOMIC_RELAXED);

                            if (!(pte & PAGE_PRESENT))
                                continue;

                            auto pp = pmm::Page_Descriptor::find_page_struct(pte & PAE_ADDR_MASK);
                            assert(pp.page_struct_ptr && "page struct must be present");
                            if (!pp.page_struct_ptr->is_anonymous())
                                continue;

                            u32 copy_from = addr_of_pd + (1 << 12) * k;
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
                                __atomic_store_n(pt + k, pte, __ATOMIC_RELEASE);
                                ctx.invalidate_page(copy_from);
                            }

                            auto result = to->map(klib::move(pp), copy_to, arg);
                            if (result)
                                return result;

                            offset = copy_from - from_addr + PAGE_SIZE;
                        }
                    }
                }
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
        u32 cr3 = new_pae_cr3();
        if (cr3 == -1U)
            return nullptr;

        auto [idle_cr3_phys, idle_cr3_offset] = cr3_pae_page_offset(idle_cr3);
        auto [cr3_phys, cr3_offset]           = cr3_pae_page_offset(cr3);

        auto guard = pmos::utility::make_scope_guard([&]() { free_pae_cr3(cr3); });
        Temp_Mapper_Obj<pae_entry_t> new_page_m(request_temp_mapper());
        Temp_Mapper_Obj<pae_entry_t> current_page_m(request_temp_mapper());

        pae_entry_t *new_page     = new_page_m.map(cr3_phys) + cr3_offset;
        pae_entry_t *current_page = current_page_m.map(idle_cr3_phys) + idle_cr3_offset;

        // Clear new page
        for (size_t i = 0; i < 4; ++i)
            __atomic_store_n(new_page + i, 0, __ATOMIC_RELAXED);

        // Copy kernel entrie
        pae_entry_t e = __atomic_load_n(current_page + 3, __ATOMIC_RELAXED);
        __atomic_store_n(new_page + 3, e, __ATOMIC_RELEASE);

        new_table->cr3 = cr3;

        auto r = insert_global_page_tables(new_table);
        if (r)
            return nullptr;

        guard.dismiss();

        return new_table;
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
        auto cr3                    = getCR3();
        auto [cr3_phys, cr3_offset] = cr3_pae_page_offset(cr3);

        Temp_Mapper_Obj<pae_entry_t> mapper(request_temp_mapper());
        pae_entry_t *pdpt = mapper.map(cr3_phys) + cr3_offset;

        auto pdpt_idx = (u32(pagefault_cr2) >> 30) & 0x3;
        auto pd_idx   = (u32(pagefault_cr2) >> 21) & 0x1FF;
        auto pt_idx   = (u32(pagefault_cr2) >> 12) & 0x1FF;

        pae_entry_t pdpt_entry = __atomic_load_n(pdpt + pdpt_idx, __ATOMIC_RELAXED);
        if (!(pdpt_entry & PAGE_PRESENT))
            return false;

        if (write && !(pdpt_entry & PAGE_WRITE))
            return false;

        if (exec && (pdpt_entry & PAGE_NX))
            return false;

        Temp_Mapper_Obj<pae_entry_t> pd_mapper(request_temp_mapper());
        pae_entry_t *pd = pd_mapper.map(pdpt_entry & PAE_ADDR_MASK);

        pae_entry_t pde = __atomic_load_n(pd + pd_idx, __ATOMIC_RELAXED);
        if (!(pde & PAGE_PRESENT))
            return false;

        if (write && !(pde & PAGE_WRITE))
            return false;

        if (exec && (pde & PAGE_NX))
            return false;

        Temp_Mapper_Obj<pae_entry_t> pt_mapper(request_temp_mapper());
        pae_entry_t *pt = pt_mapper.map(pde & PAE_ADDR_MASK);

        pae_entry_t pte = __atomic_load_n(pt + pt_idx, __ATOMIC_RELAXED);
        if (!(pte & PAGE_PRESENT))
            return false;

        if (write && !(pte & PAGE_WRITE))
            return false;

        if (exec && (pte & PAGE_NX))
            return false;

        return true;
    }
}

static u32 pae_free_list = -1U;
static void remove_from_freelist(PDPEPage *page)
{
    if (page->prev_phys == -1U) {
        pae_free_list = page->next_phys;
    } else {
        Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
        PDPEPage *prev  = mapper.map(page->prev_phys);
        prev->next_phys = page->next_phys;
    }

    if (page->next_phys != -1U) {
        Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
        PDPEPage *next  = mapper.map(page->next_phys);
        next->prev_phys = page->prev_phys;
    }
}

static void add_to_freelist(PDPEPage *page, u32 addr)
{
    page->next_phys = pae_free_list;
    page->prev_phys = -1U;
    pae_free_list   = addr;

    if (page->next_phys != -1U) {
        Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
        PDPEPage *next  = mapper.map(page->next_phys);
        next->prev_phys = addr;
    }
}

Spinlock pae_alloc_lock;

void free_pae_cr3(u32 cr3)
{
    Auto_Lock_Scope lock(pae_alloc_lock);

    u32 addr = cr3 & ~0xfff;
    u32 idx  = (cr3 & 0xfff) >> 5;

    Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
    PDPEPage *page = mapper.map(addr);

    if (idx < 64) {
        assert(page->bitmap1 & (1ULL << idx));
        page->bitmap1 &= ~(1ULL << idx);
    } else {
        assert(page->bitmap2 & (1ULL << (idx - 64)));
        page->bitmap2 &= ~(1ULL << (idx - 64));
    }

    page->allocated_count--;
    if (page->allocated_count == 0) {
        remove_from_freelist(page);
        pmm::free_memory_for_kernel(addr, 1);
    } else if (page->allocated_count == 14) {
        add_to_freelist(page, addr);
    } else {
        assert(page->allocated_count < 14);
    }
}

u32 new_pae_cr3()
{
    Auto_Lock_Scope lock(pae_alloc_lock);

    if (pae_free_list == -1U) {
        u32 addr = pmm::get_memory_for_kernel(1, kernel::pmm::AllocPolicy::Below4GB);
        if (pmm::alloc_failure(addr))
            return -1U;

        Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
        PDPEPage *page = mapper.map(addr);

        __builtin_memset(page, 0, 4096);

        page->allocated_count = 1;
        page->bitmap1         = 1;
        page->bitmap2         = 1ULL << 63; // Reserved entry

        page->next_phys = -1U;
        page->prev_phys = -1U;

        add_to_freelist(page, addr);

        return addr;
    }

    u32 addr = pae_free_list;
    Temp_Mapper_Obj<PDPEPage> mapper(request_temp_mapper());
    PDPEPage *page = mapper.map(addr);

    assert(page->allocated_count <= 14);
    assert(page->allocated_count > 0);
    if (page->allocated_count == 14) {
        remove_from_freelist(page);
    }

    u32 idx = 0;
    if (page->bitmap1 != -1ULL) {
        idx = __builtin_ffsll(~page->bitmap1) - 1;
        page->bitmap1 |= 1ULL << idx;
    } else {
        assert(page->bitmap2 != -1ULL);
        idx = __builtin_ffsll(~page->bitmap2) - 1;
        page->bitmap2 |= 1ULL << idx;
        idx += 64;
    }

    page->allocated_count++;

    __builtin_memset(page->pdpts + idx, 0, 32);

    return addr + (idx * 32);
}
