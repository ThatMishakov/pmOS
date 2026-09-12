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

#include "mem_regions.hh"

#include "mem_object.hh"
#include "paging.hh"
#include "pmm.hh"
#include "temp_mapper.hh"

#include <assert.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <pmos/memory.h>

static u64 counter = 1;

using namespace kernel;
using namespace kernel::paging;

static bool mapped_right_perm(Page_Table::Page_Info info, unsigned access_type)
{
    if (!info.is_allocated)
        return false;

    // if (access_type & Readable and not info.readable)
    //     return false;

    if ((access_type & Writeable) and (not info.writeable))
        return false;

    if ((access_type & Executable) and (not info.executable))
        return false;

    return true;
}

static bool writing(unsigned access_type) { return access_type & Writeable; }

ReturnStr<bool> Generic_Mem_Region::on_page_fault(unsigned access_type, void *pagefault_addr)
{
    pagefault_addr = (void *)((ulong)pagefault_addr & ~(ulong)(PAGE_SIZE - 1));

    if (not has_access(access_type))
        return Error(-EFAULT);

    auto mapping = owner->get_page_mapping(pagefault_addr);
    if (mapped_right_perm(mapping, access_type)) {
        // Some CPUs supposedly remember invalid pages.
        // RISC-V spec in particular says that unallocated pages can be cached
        owner->invalidate_tlb(pagefault_addr);
        return true;
    }

    if (mapping.is_allocated) {
        auto ctx = TLBShootdownContext::create_userspace(*owner);
        owner->invalidate(ctx, pagefault_addr, true);
    }

    auto page = get_page(pagefault_addr, access_type);
    if (!page.success())
        return page.propagate();

    if (!page.val)
        return false;

    assert(!(access_type & Readable) or page.val.readable);
    assert(!(access_type & Writeable) or page.val.writeable);
    assert(!(access_type & Executable) or page.val.executable);

    auto result = owner->map(page.val, pagefault_addr);
    if (result)
        return Error(result);

    return true;
}

ReturnStr<bool> Generic_Mem_Region::prepare_page(unsigned access_mode, void *page_addr)
{
    return Error(-ENOSYS);
}

Memory_Type Phys_Mapped_Region::memory_type_for_phys_addr(phys_addr_t phys_addr) const
{
    Memory_Type type;
    switch (this->type) {
    case PhysRegionType::Framebuffer:
        type = Memory_Type::Framebuffer;
        break;
    case PhysRegionType::MMIO:
        type = Memory_Type::IONoCache;
        break;
    case PhysRegionType::Normal:
        type = Memory_Type::Normal;
        break;
    default:
        type = memory_type_for_phys_addr(phys_addr);
        break;
    }
    return type;
}

ReturnStr<Page_Info> Phys_Mapped_Region::get_page(void *ptr_addr, unsigned access_type)
{
    if (not has_access(access_type))
        return Error(-EFAULT);

    phys_addr_t page_addr = (u64)ptr_addr & ~07777ULL;
    assert(page_addr >= (u64)start_addr and (u64) page_addr < (u64)start_addr + size);
    phys_addr_t phys_addr = (u64)page_addr - (u64)start_addr + phys_addr_start;

    Page_Info info = {
        .flags = PAGING_FLAG_NOFREE,
        .is_allocated = true,
        .dirty = false,
        .user_access = true,
        .nofree = true,
        .writeable = !!(access_type & Writeable),
        .executable = !!(access_type & Executable),
        .readable = !!(access_type & Readable),
        .cache_policy = memory_type_for_phys_addr(phys_addr),
        .page_addr = phys_addr,
    };
    return info;
}

kresult_t Generic_Mem_Region::move_to(TLBShootdownContext &ctx,
                                      const klib::shared_ptr<Page_Table> &new_table,
                                      void *base_addr, unsigned new_access)
{
    Page_Table *const old_owner = owner;
    auto result = old_owner->move_pages(ctx, new_table, start_addr, base_addr, size, new_access);
    if (result != 0)
        return result;

    old_owner->paging_regions.erase(this);

    owner = new_table.get();
    assert(owner);

    access_type = new_access;
    start_addr  = base_addr;

    new_table->paging_regions.insert(this);

    return 0;
}

kresult_t Phys_Mapped_Region::clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                       void *base_addr, unsigned new_access)
{
    auto copy = new Phys_Mapped_Region(*this);
    if (!copy)
        return -ENOMEM;

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    new_table->paging_regions.insert(copy);

    return 0;
}

void Phys_Mapped_Region::trim(void *new_start, size_t new_size) noexcept
{
    assert(new_start >= start_addr);
    if (new_start != start_addr) {
        phys_addr_start += (char *)new_start - (char *)start_addr;
        start_addr = new_start;
    }

    size = new_size;
}

void Mem_Object_Reference::trim(void *new_start, size_t new_size) noexcept
{
    assert(new_start >= start_addr);

    if (new_start != start_addr) {
        u64 diff = (char *)new_start - (char *)start_addr;

        object_offset_bytes += diff;
        object_size_bytes = object_size_bytes < diff ? 0 : object_size_bytes - diff;

        start_addr = new_start;
    }

    size = new_size;
    if (size < object_size_bytes)
        object_size_bytes = size;

    auto it = amap.begin();
    auto end = amap.lower_bound(object_offset_bytes);
    amap.erase(it, end);

    it = amap.lower_bound(object_offset_bytes + object_size_bytes);
    end = amap.end();
    amap.erase(it, end);
}

kresult_t Phys_Mapped_Region::punch_hole(void *hole_addr_start, size_t hole_size_bytes)
{
    assert(start_addr < hole_addr_start and
           (char *) start_addr + size > (char *)hole_addr_start + hole_size_bytes);

    auto ptr = new Phys_Mapped_Region(*this);
    if (!ptr)
        return -ENOMEM;

    ptr->size -= (char *)hole_addr_start + hole_size_bytes - (char *)start_addr;
    ptr->start_addr = (void *)((char *)hole_addr_start + hole_size_bytes);
    ptr->phys_addr_start += (char *)ptr->start_addr - (char *)start_addr;
    owner->paging_regions.insert(ptr);
    size = (char *)hole_addr_start - (char *)start_addr;

    return 0;
}

kresult_t Mem_Object_Reference::punch_hole(void *hole_addr_start, size_t hole_size_bytes)
{
    assert(start_addr < hole_addr_start and
           (char *) start_addr + size > (char *)hole_addr_start + hole_size_bytes);

    void *new_start = (char *)hole_addr_start + hole_size_bytes;
    size_t offset = (char *)new_start - (char *)start_addr;
    size_t new_size = size - offset;

    auto ptr = new Mem_Object_Reference();
    if (!ptr)
        return -ENOMEM;

    ptr->start_addr = new_start;
    ptr->size = new_size;
    // ptr->name = name;
    ptr->id = __atomic_add_fetch(&counter, 1, 0);
    ptr->owner = owner;
    ptr->access_type = access_type;
    ptr->references = references;
    ptr->object_offset_bytes = object_offset_bytes + offset;
    ptr->object_size_bytes = object_size_bytes - offset;
    ptr->cow = cow;

    for (auto it = amap.lower_bound(ptr->object_offset_bytes); it != amap.end(); ++it) {
        auto &page = it->second;
        assert(page);

        auto iit = ptr->amap.insert_noexcept({it->first, page.duplicate()});
        if (!iit.second)
            return -ENOMEM;
    }

    owner->paging_regions.insert(ptr);

    trim(start_addr, (char *)hole_addr_start - (char *)start_addr);

    return 0;
}

ReturnStr<Page_Info> Mem_Object_Reference::get_page(void *ptr_addr, unsigned access)
{
    if (not has_access(access))
        return Error(-EFAULT);

    assert((ulong)ptr_addr % PAGE_SIZE == 0);

    uintptr_t offset = (uintptr_t)ptr_addr - (uintptr_t)start_addr;
    bool is_writing = writing(access);

    if (cow) {
        auto it = amap.find(offset + object_offset_bytes);
        if (it != amap.end()) {
            auto &page = it->second;
            assert(page);

            bool owned = page.page_struct_ptr->atomic_refcount() == 1;
            
            if (owned or not is_writing) {
                bool writeable = (access_type & Writeable) and owned;

                return Page_Info{
                    .flags = PAGING_FLAG_NOFREE,
                    .is_allocated = true,
                    .dirty = false,
                    .user_access = true,
                    .nofree = true,
                    .writeable = writeable,
                    .executable = !!(access_type & Executable),
                    .readable = !!(access_type & Readable),
                    .cache_policy = Memory_Type::Normal,
                    .page_addr = page.get_phys_addr(),
                };
            }

            auto new_page = page.create_copy();
            if (!new_page.success())
                return new_page.propagate();

            auto phys_addr = new_page.val.get_phys_addr();
            it->second = std::move(new_page.val);

            return Page_Info{
                .flags = PAGING_FLAG_NOFREE,
                .is_allocated = true,
                .dirty = false,
                .user_access = true,
                .nofree = true,
                .writeable = static_cast<bool>(access_type & Writeable),
                .executable = static_cast<bool>(access_type & Executable),
                .readable = static_cast<bool>(access_type & Readable),
                .cache_policy = Memory_Type::Normal,
                .page_addr = phys_addr,
            };
        }
        
        if (offset < object_size_bytes && (offset + PAGE_SIZE <= object_size_bytes) && (!is_writing)) {
            auto page = references->atomic_request_page(offset + object_offset_bytes, false, false);
            if (!page.success())
                return page.propagate();

            if (not page.val)
                return {};

            return Page_Info{
                .flags = PAGING_FLAG_NOFREE,
                .is_allocated = true,
                .dirty = false,
                .user_access = true,
                .nofree = true,
                .writeable = false,
                .executable = static_cast<bool>(access_type & Executable),
                .readable = static_cast<bool>(access_type & Readable),
                .cache_policy = Memory_Type::Normal,
                .page_addr = page.val.get_phys_addr(),
            };
        }

        bool clear = offset >= object_size_bytes;

        auto page = references->atomic_request_anonymous_page(offset + object_offset_bytes, clear);
        if (!page.success())
            return page.propagate();

        if (not page.val)
            return {};

        auto phys_addr = page.val.get_phys_addr();
        auto res = amap.insert_noexcept({offset, std::move(page.val)});
        if (!res.second)
            return Error(-ENOMEM);

        if (offset < object_size_bytes && (offset + PAGE_SIZE > object_size_bytes)) {
            // Zero the part of the page that is outside of the object
            Temp_Mapper_Obj<char> mapper(request_temp_mapper());
            char *ptr = mapper.map(phys_addr);
            assert(ptr);

            size_t zero_start = object_size_bytes - offset;
            size_t zero_end = PAGE_SIZE;

            memset(ptr + zero_start, 0, zero_end - zero_start);
        }

        return Page_Info{
            .flags = PAGING_FLAG_NOFREE,
            .is_allocated = true,
            .dirty = false,
            .user_access = true,
            .nofree = true,
            .writeable = static_cast<bool>(access_type & Writeable),
            .executable = static_cast<bool>(access_type & Executable),
            .readable = static_cast<bool>(access_type & Readable),
            .cache_policy = Memory_Type::Normal,
            .page_addr = phys_addr,
        };
    }

    auto page = references->atomic_request_page(offset + object_offset_bytes, is_writing);
    if (!page.success())
        return page.propagate();

    if (not page.val)
        return {};

    auto phys_addr = page.val.get_phys_addr();
    return Page_Info{
        .flags = PAGING_FLAG_NOFREE,
        .is_allocated = true,
        .dirty = false,
        .user_access = true,
        .nofree = true,
        .writeable = static_cast<bool>(access_type & Writeable) && is_writing,
        .executable = static_cast<bool>(access_type & Executable),
        .readable = static_cast<bool>(access_type & Readable),
        .cache_policy = Memory_Type::Normal,
        .page_addr = phys_addr,
    };
}

kresult_t Mem_Object_Reference::move_to(TLBShootdownContext &ctx,
                                        const klib::shared_ptr<Page_Table> &new_table,
                                        void *base_addr, unsigned new_access)
{
    // This could probably be improved...
    // But for now, copy (which does CoW) and delete if successful is good enough
    auto result = clone_to(new_table, base_addr, new_access);
    if (result)
        return result;

    prepare_deletion();
    owner->paging_regions.erase(this);
    rcu_free();

    owner->invalidate_range(ctx, start_addr, size, true);
    owner->unblock_tasks_range(start_addr, size);

    return 0;
}

kresult_t Mem_Object_Reference::clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                         void *base_addr, unsigned new_access)
{
    auto copy = klib::make_unique<Mem_Object_Reference>();
    if (!copy)
        return -ENOMEM;

    copy->start_addr = base_addr;
    copy->size = size;
    copy->id = __atomic_add_fetch(&counter, 1, 0);
    copy->owner = new_table.get();
    copy->access_type = new_access;

    copy->references = references;
    copy->object_offset_bytes = object_offset_bytes;
    copy->object_size_bytes = object_size_bytes;
    copy->cow = cow;

    if (cow) {
        for (const auto &it: amap) {
            auto &page = it.second;
            assert(page);

            auto iit = copy->amap.insert_noexcept({it.first, page.duplicate()});
            if (!iit.second)
                return -ENOMEM;
        }
    }

    new_table->paging_regions.insert(copy.release());
    return 0;
}

void Generic_Mem_Region::prepare_deletion() noexcept
{
    // Do nothing
}

void Mem_Object_Reference::prepare_deletion() noexcept
{
}

Mem_Object_Reference::Mem_Object_Reference(void *start_addr, size_t size, klib::string name,
                                           Page_Table *owner, unsigned access,
                                           klib::shared_ptr<Mem_Object> references,
                                           u64 object_offset_bytes, bool copy_on_write, u64 object_size_bytes)
    : Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access),
      references(klib::forward<klib::shared_ptr<Mem_Object>>(references)),
      object_offset_bytes(object_offset_bytes),
      object_size_bytes(object_size_bytes), cow(copy_on_write)
{
    assert(cow or (object_size_bytes == size) or
           !"non-CoW region cannot have size different from the object size");
    assert((object_offset_bytes & 0xfff) == 0 or
           !"Object page-misaligned with region");
}

void Generic_Mem_Region::rcu_free() noexcept
{
    auto &rcu    = rcu_head;
    rcu.rcu_func = rcu_callback;
    rcu.rcu_next = nullptr;

    sched::get_cpu_struct()->paging_rcu_cpu.push(&rcu);
}

void Generic_Mem_Region::rcu_callback(void *ptr, bool)
{
    auto region = reinterpret_cast<Generic_Mem_Region *>((char *)ptr -
                                                         offsetof(Generic_Mem_Region, rcu_head));

    delete region;
}

Generic_Mem_Region::Generic_Mem_Region(void *start_addr, size_t size, klib::string name,
                                       Page_Table *owner, unsigned access)
    : start_addr(start_addr), size(size), name(klib::forward<klib::string>(name)),
      id(__atomic_add_fetch(&counter, 1, 0)), owner(owner), access_type(access) {};

Generic_Mem_Region::Generic_Mem_Region(): id(__atomic_add_fetch(&counter, 1, 0)) {}

PhysRegionType Phys_Mapped_Region::type_from_syscall_flags(ulong flags)
{
    switch (flags & PHYS_REGION_TYPE_MASK) {
    case PHYS_REGION_TYPE_FRAMEBUFFER:
        return PhysRegionType::Framebuffer;
    case PHYS_REGION_TYPE_MMIO:
        return PhysRegionType::MMIO;
    case PHYS_REGION_TYPE_NORMAL:
        return PhysRegionType::Normal;
    default:
        return PhysRegionType::Deduce;
    }
}