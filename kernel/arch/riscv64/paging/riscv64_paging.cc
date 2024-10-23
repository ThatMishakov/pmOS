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

#include "riscv64_paging.hh"

#include <assert.h>
#include <exceptions.hh>
#include <memory/mem_object.hh>
#include <memory/pmm.hh>
#include <memory/temp_mapper.hh>
#include <processes/tasks.hh>
#include <pmos/utility/scope_guard.hh>

using namespace kernel;

void flush_page(void *virt) noexcept { asm volatile("sfence.vma %0, x0" : : "r"(virt) : "memory"); }

bool svpbmt_enabled = false;

u8 pbmt_type(Page_Table_Argumments arg)
{
    if (!svpbmt_enabled)
        return PBMT_PMA;

    switch (arg.cache_policy) {
    case Memory_Type::Normal:
        return PBMT_PMA;
    case Memory_Type::MemoryNoCache:
        return PBMT_NC;
    case Memory_Type::IONoCache:
        return PBMT_IO;
    }

    return PBMT_PMA;
}

kresult_t riscv_map_page(u64 pt_top_phys, u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_top_phys);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table

            if (entry->valid)
                return -EEXIST;

            RISCV64_PTE new_entry = RISCV64_PTE();
            new_entry.valid       = true;
            new_entry.readable    = arg.readable;
            new_entry.writeable   = arg.writeable;
            new_entry.executable  = !arg.execution_disabled;
            new_entry.user        = arg.user_access;
            new_entry.global      = arg.global;
            new_entry.available   = arg.extra;
            new_entry.ppn         = phys_addr >> 12;
            new_entry.pbmt        = pbmt_type(arg);

            active_pt[index] = new_entry;
            return 0;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                // Allocate a new page table
                u64 new_pt_phys = pmm::get_memory_for_kernel(1);
                if (pmm::alloc_failure(new_pt_phys))
                    return -ENOMEM;

                RISCV64_PTE new_entry = RISCV64_PTE();
                new_entry.valid       = true;
                new_entry.ppn         = new_pt_phys >> 12;
                *entry                = new_entry;

                next_level_phys = new_pt_phys;
                clear_page(next_level_phys);
            } else if (entry->is_leaf()) {
                return -EEXIST;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    // 0 paging levels???
    assert(!"0 paging levels");
    return -ENOSYS;
}

kresult_t RISCV64_Page_Table::map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg)
{
    return riscv_map_page(table_root, page_addr, (void *)virt_addr, arg);
}

kresult_t RISCV64_Page_Table::map(pmm::Page_Descriptor page, u64 virt_addr,
                                  Page_Table_Argumments arg)
{
    auto pte_phys = prepare_leaf_pt_for((void *)virt_addr, arg, table_root);
    if (!pte_phys.success())
        return pte_phys.result;

    const int index = (virt_addr >> 12) & 0x1FF;

    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(pte_phys.val);

    auto &entry = active_pt[index];
    if (entry.valid)
        return -EEXIST;

    RISCV64_PTE pte = RISCV64_PTE();
    pte.valid       = true;
    pte.user        = arg.user_access;
    pte.writeable   = arg.writeable;
    pte.readable    = arg.readable;
    pte.executable  = not arg.execution_disabled;
    pte.available   = PAGING_FLAG_STRUCT_PAGE;
    pte.ppn         = page.takeout_page() >> 12;
    pte.pbmt        = 0;

    entry = pte;

    return 0;
}

kresult_t riscv_unmap_page(u64 pt_top_phys, void *virt_addr)
{
    // TODO: Return values of this function make no sense...

    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_top_phys);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table

            if (not entry->valid)
                return -EFAULT;

            RISCV64_PTE entry = active_pt[index];
            entry.clear_auto();
            entry            = RISCV64_PTE();
            active_pt[index] = entry;
            flush_page(virt_addr);

            return 0;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                return -EFAULT;
            } else if (entry->is_leaf()) {
                return -ENOSYS;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    assert(!"0 paging levels");
    return -ENOSYS;
}

void RISCV64_Page_Table::invalidate(u64 virt_addr, bool free) noexcept
{
    bool invalidated = false;
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            if (entry->valid) {
                if (free and not entry->is_special()) {
                    // Free the page
                    pmm::free_memory_for_kernel(entry->ppn << 12, 1);
                } else {
                    entry->clear_auto();
                }
                *entry      = RISCV64_PTE();
                invalidated = true;
            }
            break;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                break;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                break;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    if (invalidated)
        flush_page((void *)virt_addr);
}

bool RISCV64_Page_Table::is_mapped(u64 virt_addr) const noexcept
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            return entry->valid;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                return false;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                return false;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    return false;
}

kresult_t RISCV64_Page_Table::copy_to_recursive(const klib::shared_ptr<Page_Table> &to,
                                                u64 phys_page_level, u64 absolute_start,
                                                u64 to_addr, u64 size_bytes, u64 new_access,
                                                u64 current_copy_from, int level,
                                                u64 &upper_bound_offset)
{
    const u8 offset = 12 + (level - 1) * 9;

    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    mapper.map(phys_page_level);

    u64 mask        = (1UL << offset) - 1;
    u64 max         = (absolute_start + size_bytes + mask) & ~mask;
    u64 end_index   = (current_copy_from >> (offset + 9)) == (max >> (offset + 9))
                          ? (max >> offset) & 0x1ff
                          : 512;
    u64 start_index = (current_copy_from >> offset) & 0x1ff;

    for (u64 i = start_index; i < end_index; ++i) {
        auto pte = mapper.ptr[i];
        if (!pte.valid)
            continue;

        if (level == 1) {
            assert(pte.readable or pte.writeable or pte.executable);

            auto p = pmm::Page_Descriptor::find_page_struct(pte.ppn << 12);
            assert(p.page_struct_ptr && "page struct must be present");

            u64 copy_from = ((i - start_index) << offset) + current_copy_from;
            u64 copy_to   = copy_from - absolute_start + to_addr;

            auto copy = p.create_copy();
            if (!copy.success())
                return copy.result;

            Page_Table_Argumments arg = {
                .readable           = !!(new_access & Readable),
                .writeable          = !!(new_access & Writeable),
                .user_access        = true,
                .global             = false,
                .execution_disabled = not(new_access & Executable),
                .extra              = 0,
                // TODO: Fix this
                .cache_policy       = Memory_Type::Normal,
            };
            auto result = to->map(klib::move(copy.val), copy_to, arg);
            if (result)
                return result;

            upper_bound_offset = copy_from - absolute_start + 4096;
        } else {
            assert(!pte.readable and !pte.writeable and !pte.executable);

            u64 next_level_phys = pte.ppn << 12;
            u64 current         = ((i - start_index) << offset) + current_copy_from;
            if (i != start_index)
                current &= ~mask;
            return copy_to_recursive(to, next_level_phys, absolute_start, to_addr, size_bytes,
                                     new_access, current, level - 1, upper_bound_offset);
        }
    }

    return 0;
}

kresult_t RISCV64_Page_Table::copy_pages(const klib::shared_ptr<Page_Table> &to, u64 from_addr,
                                         u64 to_addr, u64 size_bytes, u8 access)
{
    u64 offset  = 0;
    auto result = copy_to_recursive(to, table_root, from_addr, to_addr, size_bytes, access, to_addr,
                                    riscv64_paging_levels, offset);
    if (result != 0)
        to->invalidate_range(to_addr, offset, true);

    return result;
}

// TODO: This function had great possibilities, but now seems weird
RISCV64_Page_Table::Page_Info RISCV64_Page_Table::get_page_mapping(u64 virt_addr) const
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            if (entry->valid) {
                Page_Info i {};
                i.flags        = entry->available;
                i.is_allocated = entry->valid;
                i.dirty        = entry->dirty;
                i.user_access  = entry->user;
                i.page_addr    = entry->ppn << 12;
                i.nofree       = entry->available & PAGING_FLAG_NOFREE;
                return i;
            }
            break;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                break;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                break;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }
    return Page_Info {};
}

kresult_t map_page(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr,
                   Page_Table_Argumments arg)
{
    return riscv_map_page(page_table, phys_addr, (void *)virt_addr, arg);
}

kresult_t map_pages(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, u64 size,
                    Page_Table_Argumments arg)
{
    kresult_t result = 0;

    // Don't overcomplicate things for now, just call map
    // This is *very* inefficient
    for (u64 i = 0; i < size && (result == 0); i += 4096) {
        result = map_page(page_table, phys_addr + i, virt_addr + i, arg);
    }

    // On failure, pages need to be unmapped here...
    // Leak memory for now :)

    return result;
}

kresult_t map_kernel_pages(u64 phys_addr, u64 virt_addr, u64 size, Page_Table_Argumments arg)
{
    const u64 pt_top = get_current_hart_pt();
    return map_pages(pt_top, phys_addr, virt_addr, size, arg);
}

ReturnStr<u64> prepare_leaf_pt_for(void *virt_addr, Page_Table_Argumments /* unused */, u64 pt_ptr)
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_ptr);
    for (int i = riscv64_paging_levels; i > 1; --i) {
        const u8 offset = 12 + (i - 1) * 9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];

        u64 next_level_phys;
        if (not entry->valid) {
            // Allocate a new page table
            u64 new_pt_phys = pmm::get_memory_for_kernel(1);
            if (pmm::alloc_failure(new_pt_phys))
                return Error(-ENOMEM);

            RISCV64_PTE new_entry = RISCV64_PTE();
            new_entry.valid       = true;
            new_entry.ppn         = new_pt_phys >> 12;
            *entry                = new_entry;

            next_level_phys = new_pt_phys;
            clear_page(next_level_phys);
        } else if (entry->is_leaf()) {
            return Error(-EEXIST);
        } else {
            next_level_phys = entry->ppn << 12;
        }

        active_pt = mapper.map(next_level_phys);
        if (i == 2) {
            return next_level_phys;
        }
    }

    // 0 paging levels???
    assert(!"0 paging levels");
    return 0;
}

kresult_t map_kernel_page(u64 phys_addr, void *virt_addr, Page_Table_Argumments arg)
{
    const u64 pt_top = get_current_hart_pt();
    return riscv_map_page(pt_top, phys_addr, virt_addr, arg);
}

kresult_t unmap_kernel_page(void *virt_addr)
{
    const u64 pt_top = get_current_hart_pt();
    return riscv_unmap_page(pt_top, virt_addr);
}

u64 get_current_hart_pt() noexcept
{
    u64 satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    return (satp & (((1UL << 44) - 1))) << 12;
}

void apply_page_table(ptable_top_ptr_t page_table)
{
    u64 ppn  = page_table >> 12;
    u64 asid = 0;
    u64 mode = 5 + riscv64_paging_levels;
    u64 satp = (mode << 60) | (asid << 44) | (ppn << 0);

    // Apply the new page table
    asm volatile("csrw satp, %0" : : "r"(satp) : "memory");

    // Flush the TLB
    asm volatile("sfence.vma x0, x0" : : : "memory");
}

void RISCV64_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase_if_exists(this->id);
}

kresult_t RISCV64_Page_Table::insert_global_page_tables(klib::shared_ptr<RISCV64_Page_Table> table)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    auto ret = global_page_tables.insert({table->id, table});
    if (ret.first == global_page_tables.end())
        return -ENOMEM;

    return 0;
}

RISCV64_Page_Table::page_table_map RISCV64_Page_Table::global_page_tables;
Spinlock RISCV64_Page_Table::page_table_index_lock;

void RISCV64_Page_Table::atomic_active_sum(u64 i) noexcept
{
    __atomic_add_fetch(&active_counter, i, __ATOMIC_SEQ_CST);
}

void RISCV64_Page_Table::apply() noexcept { apply_page_table(table_root); }

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::get_page_table(u64 id) noexcept
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);
    return global_page_tables.get_copy_or_default(id).lock();
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::create_clone()
{
    klib::shared_ptr<RISCV64_Page_Table> new_table = create_empty();
    if (!new_table)
        return nullptr;

    Auto_Lock_Scope_Double(this->lock, new_table->lock);

    if (!new_table->mem_objects.empty() || !new_table->object_regions.empty() || !new_table->paging_regions.empty())
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

        auto it = new_table->paging_regions.begin();
        while (it != new_table->paging_regions.end()) {
            auto region_start = it->start_addr;
            auto region_size  = it->size;

            it->prepare_deletion();
            new_table->paging_regions.erase(it);

            new_table->invalidate_range(region_start, region_size, true);

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

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::capture_initial(u64 root)
{
    klib::shared_ptr<RISCV64_Page_Table> new_table =
        klib::unique_ptr<RISCV64_Page_Table>(new RISCV64_Page_Table());

    if (!new_table)
        return nullptr;

    new_table->table_root = root;
    insert_global_page_tables(new_table);

    return new_table;
}

u64 RISCV64_Page_Table::user_addr_max() const noexcept
{
    // Of the page tables, user space gets 256 entries in the root level + other
    // levels For example, in RV39, with 3 levels of paging, the following is
    // the pointer breakdown: | all 0 | 0XXXXXXXX 256 entries (8 bits) |
    // XXXXXXXXX 512 entries (9 bits) | XXXXXXXXX 512 entries (9 bits) | 4096
    // bytes in a page, 12 bits | 12 bits + levels * 9 bits - 1 bit (half of
    // virtual memory is used by kernel)
    return 1UL << (12 + (riscv64_paging_levels * 9) - 1);
}

void RISCV64_Page_Table::invalidate_range(u64 virt_addr, u64 size_bytes, bool free)
{
    // Slow but doesn't matter for now
    u64 end = virt_addr + size_bytes;
    for (u64 i = virt_addr; i < end; i += 4096)
        invalidate(i, free);
}

RISCV64_Page_Table::~RISCV64_Page_Table()
{
    free_user_pages();
    pmm::free_memory_for_kernel(table_root, 1);
}

void free_pages_in_level(u64 pt_phys, u64 level)
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(pt_phys);

    for (u64 i = 0; i < 512; ++i) {
        RISCV64_PTE *entry = &active_pt[i];
        if (entry->valid) {
            if (level > 1) {
                assert(!entry->is_leaf());
                free_pages_in_level(entry->ppn << 12, level - 1);
            }

            entry->clear_auto();
        }
    }
}

void RISCV64_Page_Table::free_user_pages()
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(table_root);

    for (u64 i = 0; i < 256; ++i) {
        RISCV64_PTE *entry = &active_pt[i];
        if (entry->valid and not entry->is_special()) {
            assert(not entry->is_leaf());
            free_pages_in_level(entry->ppn << 12, riscv64_paging_levels - 1);
            pmm::free_memory_for_kernel(entry->ppn << 12, 1);
        }
    }
}

void RISCV64_PTE::clear_auto()
{
    if (valid and not is_special())
        pmm::free_memory_for_kernel(ppn << 12, 1);
    else if (valid and (available & PAGING_FLAG_STRUCT_PAGE)) {
        auto p = pmm::Page_Descriptor::find_page_struct(ppn << 12);
        assert(p.page_struct_ptr && "page struct must be present");
        p.release_taken_out_page();
    }

    *this = RISCV64_PTE();
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::create_empty()
{
    klib::shared_ptr<RISCV64_Page_Table> new_table =
        klib::unique_ptr<RISCV64_Page_Table>(new RISCV64_Page_Table());

    auto n = pmm::get_memory_for_kernel(1);
    if (pmm::alloc_failure(new_table->table_root))
        return nullptr;

    new_table->table_root = n;

    clear_page(new_table->table_root);

    Temp_Mapper_Obj<RISCV64_PTE> new_pt_mapper(request_temp_mapper());
    Temp_Mapper_Obj<RISCV64_PTE> current_pt_mapper(request_temp_mapper());

    RISCV64_PTE *new_pt     = new_pt_mapper.map(new_table->table_root);
    RISCV64_PTE *current_pt = current_pt_mapper.map(get_current_hart_pt());

    // Copy heap entry and kernel entries
    // Heap
    new_pt[256] = current_pt[256];

    // Kernel code
    new_pt[510] = current_pt[510];
    new_pt[511] = current_pt[511];

    auto result = new_table->insert_global_page_tables(new_table);

    if (result)
        pmm::free_memory_for_kernel(new_table->table_root, 1);

    return result ? nullptr : new_table;
}

void RISCV64_Page_Table::invalidate_tlb(u64 page) { flush_page((void *)page); }

ReturnStr<bool> RISCV64_Page_Table::atomic_copy_to_user(u64 to, const void *from, u64 size)
{
    Auto_Lock_Scope l(lock);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (u64 i = to & ~0xfffUL; i < to + size; i += 0x1000) {
        const auto b = prepare_user_page(i, Writeable);
        if (not b.success())
            return b.propagate();

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