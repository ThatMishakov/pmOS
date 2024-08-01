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
#include <kernel/com.h>
#include <kernel/memory.h>
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <utils.hh>

Page_Table::~Page_Table()
{
    for (auto &i: paging_regions)
        i.prepare_deletion();

    for (const auto &p: mem_objects)
        p.first->atomic_unregister_pined(weak_from_this());

    auto it = paging_regions.begin();
    while (it != paging_regions.end()) {
        auto next = it;
        ++next;
        paging_regions.erase(it);
        it->rcu_free();
        it = next;
    }
}

u64 Page_Table::atomic_create_normal_region(u64 page_aligned_start, u64 page_aligned_size,
                                            unsigned access, bool fixed, klib::string name,
                                            u64 pattern)
{
    Auto_Lock_Scope scope_lock(lock);

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);
    klib::unique_ptr<Generic_Mem_Region> region = new Private_Normal_Region(
        start_addr, page_aligned_size, klib::forward<klib::string>(name), this, access, pattern);

    if (fixed)
        release_in_range(start_addr, page_aligned_size);

    paging_regions.insert(region.release());

    return start_addr;
}

void Page_Table::release_in_range(u64 start_addr, u64 size)
{
    bool clean_pages = false;
    // Special case: One large region needs to be split in two, from the middle
    auto it          = paging_regions.get_smaller_or_equal(start_addr);
    if (it != paging_regions.end() and it->start_addr < start_addr and
        it->addr_end() > (start_addr + size)) {
        // This is the case when the region needs to be split
        // it is the only case that may fail because the system may not have enough memory to
        // allocate a second region

        it->punch_hole(start_addr, size);

        clean_pages = true;
    } else {
        auto it = paging_regions.get_smaller_or_equal(start_addr);
        if (it == paging_regions.end())
            it = paging_regions.begin();

        u64 end_addr = start_addr + size;
        while (it != paging_regions.end() and it->start_addr < end_addr) {
            auto i = it++;

            // 1 region can be before the one to be released
            if (not i->is_in_range(start_addr))
                continue;

            if (i->start_addr < start_addr) {
                i->trim(i->start_addr, start_addr - i->start_addr);
            } else if (i->addr_end() > end_addr) {
                i->trim(end_addr, i->addr_end() - end_addr);
            } else {
                i->prepare_deletion();
                paging_regions.erase(i);
                i->rcu_free();
            }

            clean_pages = true;
        }
    }

    if (clean_pages)
        invalidate_range(start_addr, size, true);
}

void Page_Table::atomic_release_in_range(u64 start, u64 size)
{
    Auto_Lock_Scope l(lock);
    release_in_range(start, size);
}

u64 Page_Table::atomic_transfer_region(const klib::shared_ptr<Page_Table> &to, u64 region_orig,
                                       u64 prefered_to, u64 access, bool fixed)
{
    Auto_Lock_Scope_Double scope_lock(lock, to->lock);

    try {
        auto &reg = paging_regions.at(region_orig);

        if (prefered_to & 07777)
            prefered_to = 0;

        u64 start_addr = to->find_region_spot(prefered_to, reg.size, fixed);
        reg.move_to(to, start_addr, access);
        return start_addr;
    } catch (std::out_of_range &r) {
        throw Kern_Exception(-ENOENT, "atomic_transfer_region source not found");
    }
}

u64 Page_Table::atomic_create_phys_region(u64 page_aligned_start, u64 page_aligned_size,
                                          unsigned access, bool fixed, klib::string name,
                                          u64 phys_addr_start)
{
    Auto_Lock_Scope scope_lock(lock);

    if (phys_addr_start >= phys_addr_limit() or
        phys_addr_start + page_aligned_size >= phys_addr_limit() or
        phys_addr_start > phys_addr_start + page_aligned_size)
        throw(Kern_Exception(-ENOTSUP,
                             "atomic_create_phys_region phys_addr outside the supported by x86"));

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);
    klib::unique_ptr<Generic_Mem_Region> region =
        new Phys_Mapped_Region(start_addr, page_aligned_size, klib::forward<klib::string>(name),
                               this, access, phys_addr_start);

    if (fixed)
        release_in_range(start_addr, page_aligned_size);

    paging_regions.insert(region.release());

    return start_addr;
}

u64 Page_Table::atomic_create_mem_object_region(u64 page_aligned_start, u64 page_aligned_size,
                                                unsigned access, bool fixed, klib::string name,
                                                klib::shared_ptr<Mem_Object> object, bool cow,
                                                u64 start_offset_bytes, u64 object_offset_bytes,
                                                u64 object_size_bytes)
{
    Auto_Lock_Scope scope_lock(lock);

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    auto region = klib::make_unique<Mem_Object_Reference>(
        start_addr, page_aligned_size, klib::forward<klib::string>(name), this, access, object,
        object_offset_bytes, cow, start_offset_bytes, object_size_bytes);

    if (fixed)
        release_in_range(start_addr, page_aligned_size);

    mem_objects[object].regions.insert(region.get());
    paging_regions.insert(region.release());

    return start_addr;
}

Page_Table::RegionsRBTree::RBTreeIterator Page_Table::get_region(u64 page)
{
    auto it = paging_regions.get_smaller_or_equal(page);
    if (it == paging_regions.end() or not it->is_in_range(page))
        return paging_regions.end();

    return it;
}

bool Page_Table::can_takeout_page(u64 page_addr) noexcept
{
    auto it = paging_regions.get_smaller_or_equal(page_addr);
    if (it == paging_regions.end() or not it->is_in_range(page_addr))
        return false;

    return it->can_takeout_page();
}

u64 Page_Table::find_region_spot(u64 desired_start, u64 size, bool fixed)
{
    u64 end = desired_start + size;

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
        if (fixed && desired_start == 0x0)
            throw(Kern_Exception(-EINVAL, "find_region_spot fixed region at NULL"));
        else if (fixed)
            return desired_start;

        u64 addr = 0x1000;
        auto it  = paging_regions.begin();
        while (it != paging_regions.end()) {
            u64 end = addr + size;

            if (*it > end and end <= user_addr_max())
                return addr;

            addr = it->addr_end();
            ++it;
        }

        if (addr + size > user_addr_max())
            throw(Kern_Exception(-ENOMEM, "find_region_spot no free region found"));

        return addr;
    }
}

bool Page_Table::prepare_user_page(u64 virt_addr, unsigned access_type)
{
    auto it = paging_regions.get_smaller_or_equal(virt_addr);

    if (it == paging_regions.end() or not it->is_in_range(virt_addr))
        throw(Kern_Exception(-EFAULT, "user provided parameter is unallocated"));

    return it->prepare_page(access_type, virt_addr);
}

void Page_Table::unblock_tasks(u64 page)
{
    for (const auto &it: owner_tasks)
        it.lock()->atomic_try_unblock_by_page(page);
}

void Page_Table::map(u64 page_addr, u64 virt_addr)
{
    auto it = get_region(virt_addr);
    if (it == paging_regions.end())
        throw(Kern_Exception(-EFAULT, "map no region found"));

    map(page_addr, virt_addr, it->craft_arguments());
}

u64 Page_Table::phys_addr_limit()
{
    // TODO: This is arch-specific
    return 0x01UL << 48;
}

void Page_Table::move_pages(const klib::shared_ptr<Page_Table> &to, u64 from_addr, u64 to_addr,
                            u64 size_bytes, u8 access)
{
    u64 offset = 0;

    try {
        for (; offset < size_bytes; offset += 4096) {
            auto info = get_page_mapping(from_addr + offset);
            if (info.is_allocated) {
                Page_Table_Argumments arg = {
                    !!(access & Writeable),   info.user_access, 0,
                    not(access & Executable), info.flags,
                };
                to->map(info.page_addr, to_addr + offset, arg);
            }
        }

        invalidate_range(from_addr, size_bytes, false);
    } catch (...) {
        to->invalidate_range(to_addr, offset, false);

        throw;
    }
}

void Page_Table::copy_pages(const klib::shared_ptr<Page_Table> &to, u64 from_addr, u64 to_addr,
                            u64 size_bytes, u8 access)
{
    u64 offset = 0;

    try {
        for (; offset < size_bytes; offset += 4096) {
            auto info = get_page_mapping(from_addr + offset);
            if (info.is_allocated) {
                Page_Table_Argumments arg = {
                    .readable           = !!(access & Readable),
                    .writeable          = !!(access & Writeable),
                    .user_access        = info.user_access,
                    .global             = false,
                    .execution_disabled = not(access & Executable),
                    .extra              = info.flags,
                    // TODO: Fix this
                    .cache_policy       = Memory_Type::Normal,
                };

                to->map(info.create_copy(), to_addr + offset, arg);
            }
        }
    } catch (...) {
        to->invalidate_range(to_addr, offset, true);

        throw;
    }
}

void Page_Table::atomic_pin_memory_object(klib::shared_ptr<Mem_Object> object)
{
    // TODO: I have changed the order of locking, which might cause deadlocks elsewhere
    Auto_Lock_Scope l(lock);
    bool inserted = mem_objects.insert({object, Mem_Object_Data()}).second;

    if (not inserted)
        throw Kern_Exception(-EEXIST, "memory object is already referenced");

    try {
        Auto_Lock_Scope object_lock(object->pinned_lock);
        object->register_pined(weak_from_this());
    } catch (...) {
        mem_objects.erase(object);
        throw;
    }
}

void Page_Table::atomic_shrink_regions(const klib::shared_ptr<Mem_Object> &id,
                                       u64 new_size) noexcept
{
    Auto_Lock_Scope l(lock);

    auto p  = mem_objects.at(id);
    auto it = p.regions.begin();
    while (it != p.regions.end()) {
        const auto reg = it;
        ++it;

        const auto object_end = reg->object_up_to();
        if (object_end < new_size) {
            const auto change_size_to =
                new_size > reg->start_offset_bytes ? new_size - reg->start_offset_bytes : 0UL;

            const auto free_from = reg->start_addr + change_size_to;
            const auto free_size = reg->addr_end() - free_from;

            if (change_size_to == 0) { // Delete region
                reg->prepare_deletion();
                paging_regions.erase(reg);
                reg->rcu_free();
            } else { // Resize region
                reg->size = change_size_to;
            }

            invalidate_range(free_from, free_size, true);
            unblock_tasks_rage(free_from, free_size);
        }
    }
}

void Page_Table::atomic_delete_region(u64 region_start)
{
    Auto_Lock_Scope l(lock);

    auto region = get_region(region_start);
    if (region == paging_regions.end())
        throw Kern_Exception(-ENOENT, "memory region was not found");

    auto region_size = region->size;

    region->prepare_deletion();
    paging_regions.erase(region);
    region->rcu_free();
    // paging_regions.erase(region_start);

    invalidate_range(region_start, region_size, true);
    unblock_tasks_rage(region_start, region_size);
}

void Page_Table::unreference_object(const klib::shared_ptr<Mem_Object> &object,
                                    Mem_Object_Reference *region) noexcept
{
    auto p = mem_objects.find(object);

    if (p != mem_objects.end())
        p->second.regions.erase(region);
}

void Page_Table::unblock_tasks_rage(u64 blocked_by_page, u64 size_bytes)
{
    // TODO
}