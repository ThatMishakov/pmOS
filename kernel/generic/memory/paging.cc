/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "paging.hh"

#include "assert.h"
#include "mem_object.hh"
#include "temp_mapper.hh"

#include <errno.h>
#include <kern_logger/kern_logger.hh>
#include <kernel/memory.h>
#include <lib/memory.hh>
#include <pmos/utility/scope_guard.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <utils.hh>
#include <ranges>

using namespace kernel;
using namespace kernel::paging;

Page_Table::~Page_Table()
{
    for (auto &i: paging_regions)
        i.prepare_deletion();

    auto it = paging_regions.begin();
    while (it != paging_regions.end()) {
        auto next = it;
        ++next;

        paging_regions.erase(it);
        it->rcu_free();

        it = next;
    }
}

ReturnStr<Mem_Object_Reference *>
    Page_Table::atomic_create_normal_region(void *page_aligned_start, size_t page_aligned_size,
                                            unsigned access, bool fixed, bool dma,
                                            klib::string name, u64 pattern, bool cow)
{
    unsigned flags = Mem_Object::FLAG_ANONYMOUS;
    if (dma)
        flags |= Mem_Object::FLAG_DMA;
    auto object = Mem_Object::create(12, page_aligned_size / PAGE_SIZE, flags);
    if (!object)
        return Error(-ENOMEM);

    return atomic_create_mem_object_region(page_aligned_start, page_aligned_size, access & 0x7,
                                           access & 0x8, "anonymous memory", object, cow, 0, 0,
                                           page_aligned_size);
}

ReturnStr<bool> Page_Table::atomic_copy_to_user(void *to, const void *from, size_t size)
{
    Auto_Lock_Scope l(lock);

    const ulong PAGE_MASK = ~ulong(PAGE_SIZE - 1);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (ulong i = (ulong)to & PAGE_MASK; i < (ulong)to + size; i += PAGE_SIZE) {
        const auto b = prepare_user_page((void *)i, Writeable);
        if (!b.success())
            return b.propagate();

        if (not b.val)
            return false;

        const auto page = get_page_mapping((void *)i);
        assert(page.is_allocated);

        char *ptr         = mapper.map(page.page_addr);
        const ulong start = i < (ulong)to ? (ulong)to : i;
        const ulong end   = i + PAGE_SIZE < (ulong)to + size ? i + PAGE_SIZE : (ulong)to + size;
        memcpy(ptr + (start - i), (const char *)from + (start - (ulong)to), end - start);
    }

    return true;
}

kresult_t Page_Table::release_in_range(TLBShootdownContext &ctx, void *start_addr, size_t size)
{
    bool clean_pages = false;
    // Special case: One large region needs to be split in two, from the middle
    auto it          = paging_regions.get_smaller_or_equal(start_addr);
    if (it != paging_regions.end() and it->start_addr < start_addr and
        it->addr_end() > (void *)((char *)start_addr + size)) {
        // This is the case when the region needs to be split
        // it is the only case that may fail because the system may not have enough memory to
        // allocate a second region

        auto p = it->punch_hole(start_addr, size);
        if (p)
            return p;

        clean_pages = true;
    } else {
        auto it = paging_regions.get_smaller_or_equal(start_addr);
        if (it == paging_regions.end())
            it = paging_regions.begin();

        void *end_addr = (char *)start_addr + size;
        while (it != paging_regions.end() and it->start_addr < end_addr) {
            auto i = it++;

            // 1 region can be before the one to be released
            if (not i->is_in_range(start_addr))
                continue;

            if (i->start_addr < start_addr) {
                i->trim(i->start_addr, (char *)start_addr - (char *)i->start_addr);
            } else if (i->addr_end() > end_addr) {
                i->trim(end_addr, (char *)i->addr_end() - (char *)end_addr);
            } else {
                i->prepare_deletion();
                paging_regions.erase(i);
                i->rcu_free();
            }

            clean_pages = true;
        }
    }

    if (clean_pages)
        invalidate_range(ctx, start_addr, size, true);

    return 0;
}

kresult_t Page_Table::atomic_release_in_range(void *start, size_t size)
{
    Auto_Lock_Scope l(lock);
    auto ctx = TLBShootdownContext::create_userspace(*this);
    return release_in_range(ctx, start, size);
}

ReturnStr<std::pair<void *, size_t>> Page_Table::atomic_transfer_region(const klib::shared_ptr<Page_Table> &to,
                                                     void *region_orig, void *prefered_to,
                                                     unsigned access, bool fixed)
{
    Auto_Lock_Scope_Double scope_lock(lock, to->lock);

    auto reg = paging_regions.find(region_orig);
    if (!reg)
        return Error(-ENOENT);

    auto size = reg->size;

    if ((ulong)prefered_to & 07777)
        prefered_to = nullptr;

    auto start_addr = to->find_region_spot(prefered_to, reg->size, fixed);
    if (!start_addr.success())
        return start_addr.propagate();

    auto ctx    = TLBShootdownContext::create_userspace(*this);
    auto result = reg->move_to(ctx, to, start_addr.val, access);
    if (result)
        return Error(result);

    return std::make_pair(start_addr.val, size);
}

ReturnStr<Phys_Mapped_Region *> Page_Table::atomic_create_phys_region(void *page_aligned_start,
                                                                      size_t page_aligned_size,
                                                                      unsigned access, bool fixed,
                                                                      klib::string name,
                                                                      phys_addr_t phys_addr_start,
                                                                      PhysRegionType type)
{
    Auto_Lock_Scope scope_lock(lock);

    if (phys_addr_start >= phys_addr_limit() or
        phys_addr_start + page_aligned_size >= phys_addr_limit() or
        phys_addr_start > phys_addr_start + page_aligned_size)
        return Error(-ENOTSUP);

    auto start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);
    if (!start_addr.success())
        return start_addr.propagate();

    klib::unique_ptr<Phys_Mapped_Region> region =
        new Phys_Mapped_Region(start_addr.val, page_aligned_size, klib::forward<klib::string>(name),
                               this, access, phys_addr_start, type);

    if (!region)
        return Error(-ENOMEM);

    if (fixed) {
        auto ctx = TLBShootdownContext::create_userspace(*this);
        release_in_range(ctx, start_addr.val, page_aligned_size);
    }

    paging_regions.insert(region.get());

    return region.release();
}

ReturnStr<Mem_Object_Reference *> Page_Table::atomic_create_mem_object_region(
    void *page_aligned_start, size_t page_aligned_size, unsigned access, bool fixed,
    klib::string name, klib::shared_ptr<Mem_Object> object, bool cow, u64 start_offset_bytes,
    u64 object_offset_bytes, u64 object_size_bytes) noexcept
{
    Auto_Lock_Scope scope_lock(lock);

    if (cow and object->is_dma())
        return Error(-EINVAL);

    auto start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);
    if (!start_addr.success())
        return start_addr.propagate();

    if (not cow and start_offset_bytes != 0)
        return Error(-EINVAL);

    if (not cow and object_size_bytes != page_aligned_size)
        return Error(-EINVAL);

    if ((object_offset_bytes & 0xfff) != (start_offset_bytes & 0xfff))
        return Error(-EINVAL);

    auto region = klib::make_unique<Mem_Object_Reference>(
        start_addr.val, page_aligned_size, klib::forward<klib::string>(name), this, access, object,
        object_offset_bytes, cow, start_offset_bytes, object_size_bytes);

    if (!region)
        return Error(-ENOMEM);

    if (fixed) {
        auto ctx = TLBShootdownContext::create_userspace(*this);
        auto r   = release_in_range(ctx, start_addr.val, page_aligned_size);
        if (r)
            return Error(r);
    }

    paging_regions.insert(region.get());
    return Success(region.release());
}

Page_Table::RegionsRBTree::RBTreeIterator Page_Table::get_region(void *page)
{
    auto it = paging_regions.get_smaller_or_equal(page);
    if (it == paging_regions.end() or not it->is_in_range(page))
        return paging_regions.end();

    return it;
}

bool Page_Table::can_takeout_page(void *page_addr) noexcept
{
    auto it = paging_regions.get_smaller_or_equal(page_addr);
    if (it == paging_regions.end() or not it->is_in_range(page_addr))
        return false;

    return it->can_takeout_page();
}

ReturnStr<void *> Page_Table::find_region_spot(void *desired_start, size_t size, bool fixed)
{
    void *end = (char *)desired_start + size;

    bool region_ok = desired_start != 0;

    if (region_ok) {
        auto it = paging_regions.get_smaller_or_equal(desired_start);
        if (it != paging_regions.end() and it->is_in_range(desired_start))
            region_ok = false;
    }

    if (region_ok) {
        auto it = paging_regions.lower_bound(desired_start);
        if (it != paging_regions.end() and it->start_addr < end)
            region_ok = false;
    }

    // If the new region doesn't overlap with anything, just use it
    if (region_ok) {
        return desired_start;

    } else {
        if (fixed && desired_start == nullptr)
            return Error(-EINVAL);

        else if (fixed)
            return desired_start;

        void *addr = (void *)0x1000;
        auto it    = paging_regions.begin();
        while (it != paging_regions.end()) {
            void *end = (char *)addr + size;

            if (*it > end and end <= user_addr_max())
                return addr;

            addr = it->addr_end();
            ++it;
        }

        if ((char *)addr + size > user_addr_max())
            return Error(-ENOMEM);

        return addr;
    }
}

ReturnStr<bool> Page_Table::prepare_user_page(void *virt_addr, unsigned access_type)
{
    auto it = paging_regions.get_smaller_or_equal(virt_addr);

    if (it == paging_regions.end() or not it->is_in_range(virt_addr))
        return Error(-EFAULT);

    return it->prepare_page(access_type, virt_addr);
}

void Page_Table::unblock_tasks(void *page)
{
    for (const auto &it: owner_tasks)
        it->atomic_try_unblock_by_page(page);
}

kresult_t Page_Table::map(u64 page_addr, void *virt_addr) noexcept
{
    auto it = get_region(virt_addr);
    if (it == paging_regions.end())
        return -EFAULT;

    return map(page_addr, virt_addr, it->craft_arguments(virt_addr));
}

u64 Page_Table::phys_addr_limit()
{
    // TODO: This is arch-specific
    // BIG TODO!!!
    return (u64)0x01 << 48;
}

kresult_t Page_Table::move_pages(TLBShootdownContext &ctx, const klib::shared_ptr<Page_Table> &to,
                                 void *from_addr, void *to_addr, size_t size_bytes,
                                 unsigned access) noexcept
{
    size_t offset = 0;

    pmos::utility::scope_guard guard([&]() {
        auto ctx = TLBShootdownContext::create_userspace(*to);
        to->invalidate_range(ctx, to_addr, offset, false);
    });

    for (; offset < size_bytes; offset += 4096) {
        auto info = get_page_mapping((char *)from_addr + offset);
        if (info.is_allocated) {
            Page_Table_Arguments arg = {
                .readable           = !!(access & Readable),
                .writeable          = !!(access & Writeable),
                .user_access        = info.user_access,
                .global             = 0,
                .execution_disabled = !(access & Executable),
                .extra              = info.flags,
                .cache_policy       = Memory_Type::Normal // TODO: Fix this
            };
            auto res = to->map(info.page_addr, (char *)to_addr + offset, arg);
            if (res)
                return res;
        }
    }

    invalidate_range(ctx, from_addr, size_bytes, false);

    guard.dismiss();

    return 0;
}

void Page_Table::apply_cpu(sched::CPU_Info *cpu)
{
    assert(cpu == sched::get_cpu_struct());
    assert(cpu->page_table_generation == -1); // Assert it is not in the list

    Auto_Lock_Scope l(active_cpus_lock);
    // Do it here so that waiter can check it without locking
    auto count = __atomic_add_fetch(&active_cpus_count[paging_generation], 1, __ATOMIC_RELAXED);
    assert(count > 0);

    cpu->page_table_generation = paging_generation;
    active_cpus[paging_generation].push_back(cpu);
}

void Page_Table::unapply_cpu(sched::CPU_Info *cpu)
{
    assert(cpu == sched::get_cpu_struct());
    assert(cpu->page_table_generation != -1); // Assert it is in the list

    auto generation = cpu->page_table_generation;
    Auto_Lock_Scope l(active_cpus_lock);
    auto count = __atomic_sub_fetch(&active_cpus_count[generation], 1, __ATOMIC_RELAXED);
    assert(count >= 0);

    cpu->page_table_generation = -1;
    active_cpus[generation].remove(cpu);
}

static TLBShootdownContext *kernel_shootdown_desc = nullptr;
int kernel::paging::kernel_pt_generation          = 0;

// bruh
int kernel_pt_active_cpus_count[2] = {0, 0};

void Page_Table::trigger_shootdown(Page_Table *maybe_page_table, sched::CPU_Info *cpu)
{
    if (kernel_pt_generation != cpu->kernel_pt_generation) {
        assert(kernel_pt_generation != -1);
        // Kernel shootdown

        assert(kernel_shootdown_desc != nullptr);
        auto &desc = *kernel_shootdown_desc;

        for (auto page: desc.iterate_over_pages())
            invalidate_tlb_kernel(page);

        for (auto range: desc.iterate_over_ranges())
            invalidate_tlb_kernel(range.start, range.size);

        int old_gen = cpu->kernel_pt_generation;
        int new_gen = kernel_pt_generation;

        __atomic_add_fetch(&kernel_pt_active_cpus_count[new_gen], 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&kernel_pt_active_cpus_count[old_gen], 1, __ATOMIC_RELEASE);

    } else {
        assert(cpu == sched::get_cpu_struct());
        assert(cpu->page_table_generation != -1);
        assert(maybe_page_table != nullptr);

        Auto_Lock_Scope l(maybe_page_table->active_cpus_lock);

        if (maybe_page_table->paging_generation == cpu->page_table_generation)
            return;

        auto current_generation = cpu->page_table_generation;
        auto next_generation    = current_generation == 0 ? 1 : 0;

        assert(maybe_page_table->shootdown_descriptor != nullptr);
        auto &desc = *maybe_page_table->shootdown_descriptor;

        if (desc.flush_all()) {
            maybe_page_table->tlb_flush_all();
        } else {
            for (auto page: desc.iterate_over_pages())
                maybe_page_table->invalidate_tlb(page);

            for (auto range: desc.iterate_over_ranges())
                maybe_page_table->invalidate_tlb(range.start, range.size);
        }

        if (desc.get_arch_flags())
            maybe_page_table->arch_specific_shutdown_stuff(desc.get_arch_flags());

        // Make sure the invalidation is done before the generation change
        __sync_synchronize();

        // I think the order shouldn't matter
        __atomic_add_fetch(&maybe_page_table->active_cpus_count[next_generation], 1, __ATOMIC_RELAXED);

        maybe_page_table->active_cpus[current_generation].remove(cpu);
        maybe_page_table->active_cpus[next_generation].push_back(cpu);

        cpu->page_table_generation = next_generation;

        // Not sure about the order here though
        // TODO: ???
        __atomic_sub_fetch(&maybe_page_table->active_cpus_count[current_generation], 1, __ATOMIC_RELEASE);
    }
}

kresult_t Page_Table::atomic_delete_region(void *region_start)
{
    Auto_Lock_Scope l(lock);

    auto region = get_region(region_start);
    if (region == paging_regions.end())
        return -ENOENT;

    auto region_size = region->size;

    region->prepare_deletion();
    paging_regions.erase(region);
    region->rcu_free();

    auto ctx = TLBShootdownContext::create_userspace(*this);

    invalidate_range(ctx, region_start, region_size, true);
    unblock_tasks_range(region_start, region_size);

    return 0;
}

void Page_Table::unblock_tasks_range(void *blocked_by_page, size_t size_bytes)
{
    // TODO
}

TLBShootdownContext TLBShootdownContext::create_userspace(Page_Table &page_table)
{
    // TODO: Maybe couple this with lock?
    assert(page_table.lock.is_locked());

    TLBShootdownContext ctx;
    ctx.page_table   = &page_table;
    ctx.pages_count  = 0;
    ctx.ranges_count = 0;

    return ctx;
}

TLBShootdownContext TLBShootdownContext::create_kernel()
{
    TLBShootdownContext ctx;
    ctx.page_table   = nullptr;
    ctx.pages_count  = 0;
    ctx.ranges_count = 0;

    return ctx;
}

void TLBShootdownContext::invalidate_page(void *page)
{
    constexpr phys_addr_t PAGE_MASK = ~(PAGE_SIZE - 1);
    page = (void *)((ulong)page & PAGE_MASK);

    if ((pages_count == MAX_PAGES) and for_kernel())
        finalize();

    if (pages_count >= MAX_PAGES)
        return;

    pages[pages_count] = page;    
    pages_count++;
}

bool TLBShootdownContext::flush_all() const
{
    return (pages_count >= MAX_PAGES) or (ranges_count > MAX_RANGES);
}

bool TLBShootdownContext::empty() const { return pages_count == 0 and ranges_count == 0; }
bool TLBShootdownContext::for_kernel() const { return page_table == nullptr; }

namespace kernel::sched
{
extern bool cpu_struct_works;
extern bool other_cpus_online;
} // namespace kernel::sched

void TLBShootdownContext::finalize()
{
    // TODO: This can be removed later...
    static Spinlock barrier;

    if (empty() && !arch_flags)
        return;

    assert(for_kernel() || sched::cpu_struct_works ||
           !"CPU struct is not working (kernel uninitialized) and invalidating userspace pages");

    if (for_kernel()) {
        if (!sched::cpu_struct_works || !sched::other_cpus_online) {
            // Just flush the pages and call it a day
            for (auto page: iterate_over_pages())
                invalidate_tlb_kernel(page);

            for (auto range: iterate_over_ranges())
                invalidate_tlb_kernel(range.start, range.size);
        } else {
            auto my_cpu = sched::get_cpu_struct();

            Auto_Lock_Scope l(barrier);

            int old_generation   = kernel_pt_generation;
            kernel_pt_generation = kernel_pt_generation == 0 ? 1 : 0;

            __sync_synchronize();

            for (auto cpu: sched::cpus) {
                if (cpu == my_cpu)
                    continue;
                cpu->ipi_tlb_shootdown();
            }

            Page_Table::trigger_shootdown(nullptr, my_cpu);

            while (__atomic_load_n(&kernel_pt_active_cpus_count[old_generation], __ATOMIC_ACQUIRE))
                spin_pause();
        }
    } else {
        auto my_cpu = sched::get_cpu_struct();

        int old_generation;

        Auto_Lock_Scope l(barrier);
        {
            Auto_Lock_Scope l2(page_table->active_cpus_lock);
            // flip generation and notify other CPUs
            old_generation                   = page_table->paging_generation;
            page_table->paging_generation    = page_table->paging_generation == 0 ? 1 : 0;
            page_table->shootdown_descriptor = this;

            for (auto &cpu: page_table->active_cpus[old_generation]) {
                if (&cpu == my_cpu)
                    continue;
                cpu.ipi_tlb_shootdown();
            }
        }

        // If this CPU has this page table, also flush it
        if (my_cpu->current_task->page_table.get() == page_table)
            Page_Table::trigger_shootdown(page_table, my_cpu);

        // Wait for other CPUs
        while (__atomic_load_n(&page_table->active_cpus_count[old_generation], __ATOMIC_ACQUIRE)) {
            spin_pause();
        }
    }

    // Allow finalize() to be called again
    pages_count  = 0;
    ranges_count = 0;
}

klib::vector<MemoryRegion> kernel::paging::memory_map;

bool Page_Table::is_mapped(void *ptr) const { return get_page_mapping(ptr).is_allocated; }

void kernel::paging::unmap_kernel_pages(TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes)
{
    for (size_t i = 0; i < size_bytes; i += PAGE_SIZE)
        unmap_kernel_page(ctx, (void *)((char *)virt_addr + i));
}

bool kernel::paging::region_is_usable_ram(MemoryRegionType type)
{
    return type == MemoryRegionType::Usable
           || type == MemoryRegionType::UsableReservedOnBoot
           || type == MemoryRegionType::ACPIReclaimable;
}

Memory_Type kernel::paging::memory_type_for_phys_addr(phys_addr_t phys_addr)
{
    auto it = std::ranges::upper_bound(memory_map, phys_addr, {}, &MemoryRegion::start);

    if (it == memory_map.begin())
        return Memory_Type::IONoCache;
    --it;
    if (phys_addr >= it->start + it->size)
        return Memory_Type::IONoCache;

    if (it->type == MemoryRegionType::Framebuffer)
        return Memory_Type::Framebuffer;
    else if (region_is_usable_ram(it->type))
        return Memory_Type::Normal;
    else
        return Memory_Type::IONoCache;
}