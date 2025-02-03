/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "x86_paging.hh"

#include <assert.h>
#include <cpus/ipi.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/mem_object.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <pmos/utility/scope_guard.hh>
#include <processes/tasks.hh>
#include <x86_asm.hh>

using namespace kernel;

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

    u64 pml4_entry       = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4  = mapper.map(pt_phys);
    x86_PAE_Entry &pml4e = pml4[pml4_entry];
    if (not pml4e.present) {
        pml4e  = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        pml4e.page_ppn    = p >> 12;
        pml4e.present     = 1;
        pml4e.writeable   = 1;
        pml4e.user_access = arg.user_access;
        clear_page(p);
    }

    x86_PAE_Entry *pdpt  = mapper.map(pml4e.page_ppn << 12);
    x86_PAE_Entry &pdpte = pdpt[pdpt_entry];
    if (pdpte.pat_size)
        return -EEXIST;
    if (not pdpte.present) {
        pdpte  = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        pdpte.page_ppn    = p >> 12;
        pdpte.present     = 1;
        pdpte.writeable   = 1;
        pdpte.user_access = arg.user_access;
        clear_page(p);
    }

    x86_PAE_Entry *pd  = mapper.map(pdpte.page_ppn << 12);
    x86_PAE_Entry &pde = pd[pdir_entry];
    if (pde.pat_size)
        return -EEXIST;
    if (not pde.present) {
        pde    = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        pde.page_ppn    = p >> 12;
        pde.present     = 1;
        pde.writeable   = 1;
        pde.user_access = arg.user_access;
        clear_page(p);
    }

    x86_PAE_Entry *pt  = mapper.map(pde.page_ppn << 12);
    x86_PAE_Entry &pte = pt[ptable_entry];
    if (pte.present)
        return -EEXIST;

    pte                = {};
    pte.page_ppn       = physical_addr / KB(4);
    pte.present        = 1;
    pte.user_access    = arg.user_access;
    pte.writeable      = arg.writeable;
    pte.avl            = arg.extra;
    pte.cache_disabled = arg.cache_policy != Memory_Type::Normal;
    if (nx_bit_enabled)
        pte.execution_disabled = arg.execution_disabled;
    return 0;
}

u64 get_pt_ppn(u64 virtual_addr, u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    u64 pml4_entry       = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4  = mapper.map(pt_phys);
    x86_PAE_Entry &pml4e = pml4[pml4_entry];

    x86_PAE_Entry *pdpt  = mapper.map(pml4e.page_ppn << 12);
    u64 pdpt_entry       = (virtual_addr >> 30) & 0x1ff;
    x86_PAE_Entry &pdpte = pdpt[pdpt_entry];

    x86_PAE_Entry *pd  = mapper.map(pdpte.page_ppn << 12);
    u64 pd_entry       = (virtual_addr >> 21) & 0x1ff;
    x86_PAE_Entry &pde = pd[pd_entry];

    return pde.page_ppn;
}

kresult_t map_pages(u64 cr3, u64 phys_addr, u64 virt_addr, u64 size_bytes,
                    Page_Table_Argumments args)
{
    u64 result = 0;
    u64 i      = 0;

    // TODO: Unmap on failure

    for (; i < size_bytes; i += 0x1000) {
        result = map(phys_addr + i, virt_addr + i, args, cr3);
        if (result)
            return result;
    }

    return result;
}

u64 idle_cr3 = 0;

kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    return map(phys_addr, (u64)virt_addr, arg, idle_cr3);
}

u64 prepare_pt_for(u64 virtual_addr, Page_Table_Argumments arg, u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    u64 pml4_entry       = (virtual_addr >> 39) & 0x1ff;
    x86_PAE_Entry *pml4  = mapper.map(pt_phys);
    x86_PAE_Entry &pml4e = pml4[pml4_entry];
    if (not pml4e.present) {
        pml4e  = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -1;

        pml4e.page_ppn    = p >> 12;
        pml4e.present     = 1;
        pml4e.writeable   = 1;
        pml4e.user_access = arg.user_access;
        clear_page(p);
    }

    x86_PAE_Entry *pdpt  = mapper.map(pml4e.page_ppn << 12);
    u64 pdpt_entry       = (virtual_addr >> 30) & 0x1ff;
    x86_PAE_Entry &pdpte = pdpt[pdpt_entry];
    if (pdpte.pat_size)
        return -1;
    if (not pdpte.present) {
        pdpte  = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -1;

        pdpte.page_ppn    = p >> 12;
        pdpte.present     = 1;
        pdpte.writeable   = 1;
        pdpte.user_access = arg.user_access;
        clear_page(p);
    }

    x86_PAE_Entry *pd  = mapper.map(pdpte.page_ppn << 12);
    u64 pd_entry       = (virtual_addr >> 21) & 0x1ff;
    x86_PAE_Entry &pde = pd[pd_entry];
    if (pde.pat_size)
        return -1;
    if (not pde.present) {
        pde    = {};
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -1;

        pde.page_ppn    = p >> 12;
        pde.present     = 1;
        pde.writeable   = 1;
        pde.user_access = arg.user_access;
        clear_page(p);
    }

    return pde.page_ppn << 12;
}

bool x86_4level_Page_Table::is_mapped(u64 virt_addr) const
{
    bool allocated = false;

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    do {
        mapper.map((u64)pml4_phys);
        x86_PAE_Entry *pml4e = &mapper.ptr[pml4_index(virt_addr)];
        if (not pml4e->present)
            break;

        mapper.map((u64)pml4e->page_ppn << 12);
        x86_PAE_Entry *pdpte = &mapper.ptr[pdpt_index(virt_addr)];
        if (not pdpte->present)
            break;

        mapper.map((u64)pdpte->page_ppn << 12);
        x86_PAE_Entry *pde = &mapper.ptr[pd_index(virt_addr)];
        if (not pde->present)
            break;

        mapper.map((u64)pde->page_ppn << 12);
        x86_PAE_Entry *pte = &mapper.ptr[pt_index(virt_addr)];
        allocated          = pte->present;
    } while (false);

    return allocated;
}

void invalidate(TLBShootdownContext &ctx, u64 virt_addr, bool free, u64 pml4_phys)
{
    bool invalidated = false;

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    do {
        mapper.map((u64)pml4_phys);
        x86_PAE_Entry *pml4e = &mapper.ptr[x86_4level_Page_Table::pml4_index(virt_addr)];
        if (not pml4e->present)
            break;

        mapper.map((u64)pml4e->page_ppn << 12);
        x86_PAE_Entry *pdpte = &mapper.ptr[x86_4level_Page_Table::pdpt_index(virt_addr)];
        if (not pdpte->present)
            break;

        mapper.map((u64)pdpte->page_ppn << 12);
        x86_PAE_Entry *pde = &mapper.ptr[x86_4level_Page_Table::pd_index(virt_addr)];
        if (not pde->present)
            break;

        mapper.map((u64)pde->page_ppn << 12);
        x86_PAE_Entry *pte = &mapper.ptr[x86_4level_Page_Table::pt_index(virt_addr)];
        if (not pte->present)
            break;

        if (free)
            pte->clear_auto();
        else
            pte->clear_nofree();

        invalidated = true;
    } while (false);

    if (invalidated)
        ctx.invalidate_page(virt_addr);
}

void x86_4level_Page_Table::invalidate(TLBShootdownContext &ctx, u64 virt_addr, bool free)
{
    ::invalidate(ctx, virt_addr, free, (u64)pml4_phys);
}

kresult_t unmap_kernel_page(TLBShootdownContext &ctx, void *virt_addr)
{
    invalidate(ctx, (u64)virt_addr, false, idle_cr3);
    return 0;
}

bool x86_Page_Table::is_used_by_others() const { return (active_count - is_active()) != 0; }

bool x86_Page_Table::is_active() const { return ::getCR3() == get_cr3(); }

// void x86_Page_Table::invalidate_tlb(u64 page, u64 size)
// {
//     if (is_active())
//         for (u64 i = 0; i < size; i += 4096)
//             invlpg(page + i);

//     if (is_used_by_others())
//         ::signal_tlb_shootdown();
// }

void x86_Page_Table::invalidate_tlb(u64 page, u64 size)
{
    if (size / 4096 < 64)
        for (u64 i = 0; i < size; i += 4096)
            invlpg(page + i);

    else
        setCR3(get_cr3());
}

void x86_Page_Table::tlb_flush_all() { setCR3(get_cr3()); }

void x86_Page_Table::atomic_active_sum(u64 val) noexcept
{
    __atomic_add_fetch(&active_count, val, 0);
}

void print_pt(u64 addr)
{
    u64 ptable_entry = addr >> 12;
    u64 pd_e         = ptable_entry >> 9;
    u64 pdpe         = pd_e >> 9;
    u64 pml4e        = pdpe >> 9;
    u64 upper        = pml4e >> 9;

    global_logger.printf("Paging indexes %h\'%h\'%h\'%h\'%h\n", upper, pml4e & 0777, pdpe & 0777,
                         pd_e & 0777, ptable_entry & 0777);
}

ReturnStr<u64> x86_4level_Page_Table::phys_addr_of(u64 virt) const
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    // PML4 entry
    mapper.map((u64)pml4_phys);
    unsigned pml4_i = pml4_index(virt);
    if (not mapper.ptr[pml4_i].present)
        return -EFAULT;

    // PDPT entry
    mapper.map(mapper.ptr[pml4_i].page_ppn << 12);
    unsigned pdpt_i = pdpt_index(virt);
    if (not mapper.ptr[pdpt_i].present)
        return -EFAULT;

    // PD entry
    mapper.map(mapper.ptr[pdpt_i].page_ppn << 12);
    unsigned pd_i = pd_index(virt);
    if (not mapper.ptr[pd_i].present)
        return -EFAULT;

    // PT entry
    mapper.map(mapper.ptr[pd_i].page_ppn << 12);
    unsigned pt_i = pt_index(virt);
    if (not mapper.ptr[pt_i].present)
        return -EFAULT;

    return (mapper.ptr[pt_i].page_ppn << 12) | (virt & (u64)0xfff);
}

x86_4level_Page_Table::Page_Info x86_4level_Page_Table::get_page_mapping(u64 virt_addr) const
{
    Page_Info i {};

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());

    // PML4 entry
    mapper.map((u64)pml4_phys);
    unsigned pml4_i = pml4_index(virt_addr);
    if (not mapper.ptr[pml4_i].present)
        return i;

    // PDPT entry
    mapper.map(mapper.ptr[pml4_i].page_ppn << 12);
    unsigned pdpt_i = pdpt_index(virt_addr);
    if (not mapper.ptr[pdpt_i].present)
        return i;

    // PD entry
    mapper.map(mapper.ptr[pdpt_i].page_ppn << 12);
    unsigned pd_i = pd_index(virt_addr);
    if (not mapper.ptr[pd_i].present)
        return i;

    // PT entry
    mapper.map(mapper.ptr[pd_i].page_ppn << 12);
    unsigned pt_i  = pt_index(virt_addr);
    i.flags        = mapper.ptr[pt_i].avl;
    i.is_allocated = mapper.ptr[pt_i].present;
    i.writeable    = mapper.ptr[pt_i].writeable;
    i.executable   = not mapper.ptr[pt_i].execution_disabled;
    i.dirty        = mapper.ptr[pt_i].dirty;
    i.user_access  = mapper.ptr[pt_i].user_access;
    i.page_addr    = mapper.ptr[pt_i].page_ppn << 12;
    i.nofree       = mapper.ptr[pt_i].avl & PAGING_FLAG_NOFREE;
    return i;
}

ReturnStr<u64> phys_addr_of(u64 virt)
{
    PTE *pte = get_pte(virt, rec_map_index);

    u64 phys = (pte->page_ppn << 12) | (virt & (u64)0xfff);

    // TODO: Error checking
    return {0, phys};
}

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::capture_initial(u64 cr3)
{
    klib::shared_ptr<x86_4level_Page_Table> t =
        klib::unique_ptr<x86_4level_Page_Table>(new x86_4level_Page_Table());

    if (not t)
        return nullptr;

    t->pml4_phys = cr3;
    auto r       = insert_global_page_tables(t);
    if (r)
        return nullptr;

    return t;
}

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::create_empty(int flags)
{
    klib::shared_ptr<x86_4level_Page_Table> new_table =
        klib::unique_ptr<x86_4level_Page_Table>(new x86_4level_Page_Table());

    if (not new_table)
        return nullptr;

    new_table->flags = flags;

    // Get a free page
    pmm::Page::page_addr_t p = -1;

    p = pmm::get_memory_for_kernel(1);
    if (pmm::alloc_failure(p))
        return nullptr;

    auto guard = pmos::utility::make_scope_guard([&]() { pmm::free_memory_for_kernel(p, 1); });

    // Map pages
    Temp_Mapper_Obj<x86_PAE_Entry> new_page_m(request_temp_mapper());
    Temp_Mapper_Obj<x86_PAE_Entry> current_page_m(request_temp_mapper());

    new_page_m.map(p);
    current_page_m.map(idle_cr3);

    // Clear it as memory contains rubbish and it will cause weird paging
    // bugs on real machines
    page_clear((void *)new_page_m.ptr);

    // Copy the last entries into the new page table as they are shared
    // across all processes
    memcpy(new_page_m.ptr + 256, current_page_m.ptr + 256, 256 * sizeof(x86_PAE_Entry));

    new_table->pml4_phys = p;

    auto r = insert_global_page_tables(new_table);
    if (r)
        return nullptr;

    guard.dismiss();

    return new_table;
}

x86_4level_Page_Table::~x86_4level_Page_Table()
{
    takeout_global_page_tables();

    if (pml4_phys != -1UL) {
        free_user_pages();
        pmm::free_memory_for_kernel(pml4_phys, 1);
    }
}

kresult_t x86_4level_Page_Table::map(u64 physical_addr, u64 virtual_addr,
                                     Page_Table_Argumments arg) noexcept
{
    assert(!(physical_addr >> 48) && "x86_4level_Page_Table::map physical page out of range");

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map((u64)pml4_phys);

    x86_PAE_Entry *pml4e = &mapper.ptr[pml4_index(virtual_addr)];
    if (not pml4e->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pml4e             = x86_PAE_Entry();
        pml4e->page_ppn    = p >> 12;
        pml4e->present     = 1;
        pml4e->writeable   = 1;
        pml4e->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pml4e->page_ppn << 12);
    x86_PAE_Entry *pdpte = &mapper.ptr[pdpt_index(virtual_addr)];
    if (pdpte->pat_size) // 1GB page is already present
        return -EEXIST;

    if (not pdpte->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pdpte             = x86_PAE_Entry();
        pdpte->page_ppn    = p >> 12;
        pdpte->present     = 1;
        pdpte->writeable   = 1;
        pdpte->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pdpte->page_ppn << 12);
    x86_PAE_Entry *pde = &mapper.ptr[pd_index(virtual_addr)];
    if (pde->pat_size)
        return -EEXIST;

    if (not pde->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pde             = x86_PAE_Entry();
        pde->page_ppn    = p >> 12;
        pde->present     = 1;
        pde->writeable   = 1;
        pde->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pde->page_ppn << 12);
    x86_PAE_Entry *pte = &mapper.ptr[pt_index(virtual_addr)];
    if (pte->present)
        return -EEXIST;

    assert(!(arg.extra & PAGING_FLAG_STRUCT_PAGE) ||
           pmm::Page_Descriptor::find_page_struct(physical_addr));

    *pte                = x86_PAE_Entry();
    pte->page_ppn       = physical_addr / KB(4);
    pte->present        = 1;
    pte->user_access    = arg.user_access;
    pte->writeable      = arg.writeable;
    pte->avl            = arg.extra;
    pte->cache_disabled = arg.cache_policy != Memory_Type::Normal;
    if (nx_bit_enabled)
        pte->execution_disabled = arg.execution_disabled;

    return 0;
}

kresult_t x86_4level_Page_Table::map(pmm::Page_Descriptor page, u64 virtual_addr,
                                     Page_Table_Argumments arg) noexcept
{
    auto p = page.page_struct_ptr;
    assert(p && "page must be present");
    assert(p->type == pmm::Page::PageType::Allocated);

    if (page.page_struct_ptr->get_phys_addr() >> 48)
        assert(false && "x86_4level_Page_Table::map physical page out of range");

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map((u64)pml4_phys);

    x86_PAE_Entry *pml4e = &mapper.ptr[pml4_index(virtual_addr)];
    if (not pml4e->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pml4e             = x86_PAE_Entry();
        pml4e->page_ppn    = p >> 12;
        pml4e->present     = 1;
        pml4e->writeable   = 1;
        pml4e->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pml4e->page_ppn << 12);
    x86_PAE_Entry *pdpte = &mapper.ptr[pdpt_index(virtual_addr)];
    if (pdpte->pat_size) // 1GB page is already present
        return -EEXIST;

    if (not pdpte->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pdpte             = x86_PAE_Entry();
        pdpte->page_ppn    = p >> 12;
        pdpte->present     = 1;
        pdpte->writeable   = 1;
        pdpte->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pdpte->page_ppn << 12);
    x86_PAE_Entry *pde = &mapper.ptr[pd_index(virtual_addr)];
    if (pde->pat_size)
        return -EEXIST;

    if (not pde->present) {
        auto p = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(p))
            return -ENOMEM;

        *pde             = x86_PAE_Entry();
        pde->page_ppn    = p >> 12;
        pde->present     = 1;
        pde->writeable   = 1;
        pde->user_access = arg.user_access;
        clear_page(p);
    }

    mapper.map(pde->page_ppn << 12);
    x86_PAE_Entry *pte = &mapper.ptr[pt_index(virtual_addr)];
    if (pte->present)
        return -EEXIST;

    *pte                = x86_PAE_Entry();
    pte->present        = 1;
    pte->user_access    = arg.user_access;
    pte->writeable      = arg.writeable;
    pte->avl            = PAGING_FLAG_STRUCT_PAGE;
    pte->cache_disabled = arg.cache_policy != Memory_Type::Normal;
    u64 page_ppn        = page.takeout_page() >> 12;
    pte->page_ppn       = page_ppn;

    if (nx_bit_enabled)
        pte->execution_disabled = arg.execution_disabled;

    return 0;
}

kresult_t x86_4level_Page_Table::resolve_anonymous_page(u64 virt_addr, u64 access_type)
{
    assert(access_type & Writeable);

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(pml4_phys);
    for (int i = 3; i > 0; --i) {
        int offset      = 12 + i * 9;
        int index       = (virt_addr >> offset) & 0x1ff;
        x86_PAE_Entry p = mapper.ptr[index];
        assert(p.present && "page must be present");
        mapper.map(p.page_ppn << 12);
    }

    auto i          = pt_index(virt_addr);
    x86_PAE_Entry p = mapper.ptr[i];
    assert(p.present && "page must be present");
    assert(p.avl & PAGING_FLAG_STRUCT_PAGE && "page must be a struct page");
    assert(!p.writeable && "page must be read-only");

    auto page = pmm::Page_Descriptor::find_page_struct(p.page_ppn << 12);
    assert(page.page_struct_ptr && "page struct not found");

    if (__atomic_load_n(&page.page_struct_ptr->l.refcount, __ATOMIC_ACQUIRE) == 2) {
        // only owner of the page
        p.writeable   = 1;
        mapper.ptr[i] = p;
        return 0;
    }

    auto owner = page.page_struct_ptr->l.owner;
    assert(owner && "page owner not found");

    auto new_descriptor =
        owner->atomic_request_anonymous_page(page.page_struct_ptr->l.offset, true);
    if (!new_descriptor.success())
        return new_descriptor.result;

    p.present     = 0;
    mapper.ptr[i] = p;
    // Fun!
    {
        auto tlb_ctx = TLBShootdownContext::create_userspace(*this);
        tlb_ctx.invalidate_page(virt_addr & ~0xffful);
    }

    u64 new_page_phys = new_descriptor.val.takeout_page();

    Temp_Mapper_Obj<void> old_page(request_temp_mapper());
    old_page.map(p.page_ppn << 12);
    Temp_Mapper_Obj<void> new_page(request_temp_mapper());
    new_page.map(new_page_phys);

    __builtin_memcpy(new_page.ptr, old_page.ptr, 4096);

    page.release_taken_out_page();

    p.page_ppn    = new_page_phys >> 12;
    p.writeable   = 1;
    p.present     = 1;
    mapper.ptr[i] = p;

    return 0;
}

void x86_4level_Page_Table::free_pt(u64 pt_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(pt_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry *p = &mapper.ptr[i];
        p->clear_auto();
    }
}

void x86_4level_Page_Table::free_pd(u64 pd_phys)
{
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(pd_phys);

    for (u64 i = 0; i < 512; ++i) {
        x86_PAE_Entry *p = &mapper.ptr[i];
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
        x86_PAE_Entry *p = &mapper.ptr[i];
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
        x86_PAE_Entry *p = &mapper.ptr[i];
        if (p->present) {
            free_pdpt(p->page_ppn << 12);
            pmm::free_memory_for_kernel(p->page_ppn << 12, 1);
        }
    }
}

void x86_4level_Page_Table::invalidate_range(TLBShootdownContext &ctx, u64 virt_addr,
                                             u64 size_bytes, bool free)
{
    // -------- CAN BE MADE MUCH FASTER ------------
    u64 end = virt_addr + size_bytes;
    for (u64 i = virt_addr; i < end; i += 4096)
        invalidate(ctx, i, free);
}

void x86_PAE_Entry::clear_nofree() { *this = {}; }

void x86_PAE_Entry::clear_auto()
{
    if (not present) {
        *this = x86_PAE_Entry();
        return;
    }

    if (avl & PAGING_FLAG_STRUCT_PAGE) {
        auto p = pmm::Page_Descriptor::find_page_struct(page_ppn << 12);
        assert(p.page_struct_ptr && "page struct must be present");
        p.release_taken_out_page();
    } else if (not(avl & PAGING_FLAG_NOFREE))
        pmm::free_memory_for_kernel(page_ppn << 12, 1);

    *this = x86_PAE_Entry();
}

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::create_clone()
{
    klib::shared_ptr<x86_4level_Page_Table> new_table = create_empty();
    if (not new_table)
        return nullptr;

    Auto_Lock_Scope_Double lock_guard(this->lock, new_table->lock);

    if (new_table->mem_objects.size() != 0)
        // Somebody has messed with the page table while it was being created
        // I don't know if it's the best solution to not block the tables
        // immediately but I believe it's better to block them for shorter time
        // and abort the operation when someone tries to mess with the paging,
        // which would either be very poor coding or a bug anyways
        return nullptr;

    auto guard = pmos::utility::make_scope_guard([&]() {
        // Remove all the regions and objects. It might not be necessary, since
        // it should be handled by the destructor but in case somebody from
        // userspace does weird stuff with the
        // not-yet-fully-constructed page table, it's better to give them an
        // empty table

        for (const auto &reg: new_table->mem_objects)
            reg.first->atomic_unregister_pined(new_table->weak_from_this());

        auto ctx = TLBShootdownContext::create_userspace(*new_table);

        auto it = new_table->paging_regions.begin();
        while (it != new_table->paging_regions.end()) {
            auto region_start = it->start_addr;
            auto region_size  = it->size;

            it->prepare_deletion();
            paging_regions.erase(it);
            it->rcu_free();

            invalidate_range(ctx, region_start, region_size, true);

            it = new_table->paging_regions.begin();
        }
    });

    for (auto &reg: this->mem_objects) {
        auto t = new_table->mem_objects.insert(
            klib::pair<const klib::shared_ptr<Mem_Object>, Mem_Object_Data> {
                reg.first,
                {
                    .max_privilege_mask = reg.second.max_privilege_mask,
                }});
        if (not t.second)
            return nullptr;

        Auto_Lock_Scope reg_lock(reg.first->pinned_lock);
        auto r = reg.first->register_pined(new_table->weak_from_this());
        if (r)
            return nullptr;
    }

    for (auto &reg: this->paging_regions) {
        auto r = reg.clone_to(new_table, reg.start_addr, reg.access_type);
        if (r)
            return nullptr;
    }

    guard.dismiss();

    return new_table;
}

x86_4level_Page_Table::page_table_map x86_4level_Page_Table::global_page_tables;
Spinlock x86_4level_Page_Table::page_table_index_lock;

kresult_t
    x86_4level_Page_Table::insert_global_page_tables(klib::shared_ptr<x86_4level_Page_Table> table)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    auto p = global_page_tables.insert({table->id, table});
    if (p.first == global_page_tables.end()) [[unlikely]]
        return -ENOMEM;

    if (not p.second) [[unlikely]]
        return -EEXIST;

    return 0;
}

void x86_4level_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase_if_exists(this->id);
}

void x86_4level_Page_Table::apply() noexcept { setCR3((u64)pml4_phys); }

void apply_page_table(ptable_top_ptr_t page_table) { setCR3((u64)page_table); }

void x86_Page_Table::invalidate_tlb(u64 addr) { invlpg(addr); }
void invalidate_tlb_kernel(u64 addr) { invlpg(addr); }

void invalidate_tlb_kernel(u64 addr, u64 size)
{
    for (u64 i = 0; i < size; i += 4096)
        invlpg(addr + i);
}

ReturnStr<bool> x86_4level_Page_Table::atomic_copy_to_user(u64 to, const void *from, u64 size)
{
    Auto_Lock_Scope l(lock);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (u64 i = to & ~0xfffUL; i < to + size; i += 0x1000) {
        const auto b = prepare_user_page(i, Writeable);
        if (!b.success())
            b.propagate();

        if (not b.val)
            return false;

        const auto page = get_page_mapping(i);
        assert(page.is_allocated);

        char *ptr       = mapper.map(page.page_addr);
        const u64 start = i < to ? to : i;
        const u64 end   = i + 0x1000 < to + size ? i + 0x1000 : to + size;
        memcpy(ptr + (start - i), (const char *)from + (start - to), end - start);
    }

    return true;
}

klib::shared_ptr<x86_4level_Page_Table> x86_4level_Page_Table::get_page_table(u64 id)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    auto p = global_page_tables.find(id);
    if (p == global_page_tables.end())
        return nullptr;

    return p->second.lock();
}

kresult_t x86_4level_Page_Table::copy_to_recursive(const klib::shared_ptr<Page_Table> &to,
                                                   u64 phys_page_level, u64 absolute_start,
                                                   u64 to_addr, u64 size_bytes, u64 new_access,
                                                   u64 current_copy_from, int level,
                                                   u64 &upper_bound_offset,
                                                   TLBShootdownContext &ctx)
{
    const u8 offset = 12 + (level - 1) * 9;

    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(phys_page_level);

    u64 mask        = (1UL << offset) - 1;
    u64 max         = (absolute_start + size_bytes + mask) & ~mask;
    u64 end_index   = (current_copy_from >> (offset + 9)) == (max >> (offset + 9))
                          ? (max >> offset) & 0x1ff
                          : 512;
    u64 start_index = (current_copy_from >> offset) & 0x1ff;

    for (u64 i = start_index; i < end_index; ++i) {
        auto p = mapper.ptr[i];
        if (not p.present)
            continue;

        if (level == 1) {
            auto pp = pmm::Page_Descriptor::find_page_struct(p.page_ppn << 12);
            assert(pp.page_struct_ptr && "page struct must be present");
            if (!pp.page_struct_ptr->is_anonymous())
                continue;

            u64 copy_from = ((i - start_index) << offset) + current_copy_from;
            u64 copy_to   = copy_from - absolute_start + to_addr;

            // Spelling mistake?
            Page_Table_Argumments arg = {
                .readable           = true,
                .writeable          = false,
                .user_access        = p.user_access,
                .global             = p.global,
                .execution_disabled = p.execution_disabled,
                .extra              = p.avl,
                .cache_policy = p.cache_disabled ? Memory_Type::IONoCache : Memory_Type::Normal,
            };

            if (p.writeable) {
                p.writeable   = 0;
                mapper.ptr[i] = p;
                ctx.invalidate_page(copy_from);
            }

            auto result = to->map(klib::move(pp), copy_to, arg);
            if (result)
                return result;

            upper_bound_offset = copy_from - absolute_start + 4096;
        } else {
            assert(p.pat_size == 0 && "huge pages are not supported");

            u64 new_page_phys = p.page_ppn << 12;
            u64 current       = ((i - start_index) << offset) + current_copy_from;
            if (i != start_index)
                current &= ~mask;
            auto result =
                copy_to_recursive(to, new_page_phys, absolute_start, to_addr, size_bytes,
                                  new_access, current, level - 1, upper_bound_offset, ctx);
            if (result)
                return result;
        }
    }

    return 0;
}

kresult_t x86_4level_Page_Table::copy_anonymous_pages(const klib::shared_ptr<Page_Table> &to,
                                                      u64 from_addr, u64 to_addr, u64 size_bytes,
                                                      u8 access)
{
    u64 offset = 0;

    auto guard = pmos::utility::make_scope_guard([&]() {
        auto ctx = TLBShootdownContext::create_userspace(*to);
        to->invalidate_range(ctx, to_addr, offset, true);
    });

    kresult_t result;

    {
        TLBShootdownContext ctx = TLBShootdownContext::create_userspace(*this);
        result = copy_to_recursive(to, pml4_phys, from_addr, to_addr, size_bytes, access, from_addr,
                                   4, offset, ctx);
    }

    if (result == 0)
        guard.dismiss();

    return result;
}

static bool check_level(void *ptr, unsigned level, u64 phys_page_level, ulong err)
{
    bool write = err & 0x02;
    bool exec  = err & 0x10;
    if (level == 0)
        assert(!"level 0");

    unsigned idx = ((ulong)ptr >> (12 + (level - 1) * 9)) & 0x1ff;
    Temp_Mapper_Obj<x86_PAE_Entry> mapper(request_temp_mapper());
    mapper.map(phys_page_level);

    auto pte = mapper.ptr[idx];
    if (!pte.present)
        return false;

    if (write && !pte.writeable)
        return false;

    if (exec && pte.execution_disabled)
        return false;

    if (level == 1)
        return true;

    return check_level(ptr, level - 1, pte.page_ppn << 12, err);
}

bool page_mapped(void *pagefault_cr2, ulong err)
{

    unsigned levels = 4; // TODO;

    auto cr3 = getCR3();

    return check_level(pagefault_cr2, levels, cr3, err);
}