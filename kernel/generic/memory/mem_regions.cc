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
#include <kern_logger/kern_logger.hh>
#include <pmos/ipc.h>
#include <processes/tasks.hh>
#include <sched/sched.hh>
u64 counter = 1;

using namespace kernel;

ReturnStr<bool> Generic_Mem_Region::on_page_fault(u64 access_type, u64 pagefault_addr)
{
    if (not has_access(access_type))
        return Error(-EFAULT);

    if (owner->is_mapped(pagefault_addr)) {
        // Some CPUs supposedly remember invalid pages.
        // RISC-V spec in particular says that unallocated pages can be cached
        owner->invalidate_tlb(pagefault_addr);
        return true;
    }

    return alloc_page(pagefault_addr);
}

ReturnStr<bool> Generic_Mem_Region::prepare_page(u64 access_mode, u64 page_addr)
{
    if (not has_access(access_mode))
        return Error(-EFAULT);

    if (owner->is_mapped(page_addr))
        return true;

    return alloc_page(page_addr);
}

Page_Table_Argumments Private_Normal_Region::craft_arguments() const
{
    return {
        !!(access_type & Readable),
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable),
        0b000,
    };
}

ReturnStr<bool> Private_Normal_Region::alloc_page(u64 ptr_addr)
{
    auto page = pmm::Page_Descriptor::allocate_page_zeroed(12);
    if (!page.success())
        return page.propagate();

    auto result = owner->map(klib::move(page.val), ptr_addr, craft_arguments());
    if (result)
        return Error(result);

    return true;
}

Page_Table_Argumments Phys_Mapped_Region::craft_arguments() const
{
    return {
        !!(access_type & Readable),
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable),
        0b010,
        // TODO: This is temporary, make it a flag
        Memory_Type::IONoCache,
    };
}

ReturnStr<bool> Phys_Mapped_Region::alloc_page(phys_addr_t ptr_addr)
{
    Page_Table_Argumments args = craft_arguments();

    phys_addr_t page_addr = (u64)ptr_addr & ~07777UL;
    phys_addr_t phys_addr = page_addr - start_addr + phys_addr_start;

    auto result = owner->map(phys_addr, page_addr, args);
    if (result)
        return Error(result);

    return true;
}

kresult_t Generic_Mem_Region::move_to(TLBShootdownContext &ctx, const klib::shared_ptr<Page_Table> &new_table,
                                      size_t base_addr, size_t new_access)
{
    Page_Table *const old_owner = owner;
    auto result = old_owner->move_pages(ctx, new_table, start_addr, base_addr, size, new_access);
    if (result != 0)
        return result;

    old_owner->paging_regions.erase(this);
    new_table->paging_regions.insert(this);

    owner = new_table.get();
    if (!owner)
        return -EINVAL;

    access_type = new_access;
    start_addr  = base_addr;

    return 0;
}

kresult_t Phys_Mapped_Region::clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                       size_t base_addr, u64 new_access)
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

void Phys_Mapped_Region::trim(u64 new_start, u64 new_size) noexcept
{
    assert(new_start >= start_addr);
    if (new_start != start_addr) {
        phys_addr_start += new_start - start_addr;
        start_addr = new_start;
    }

    size = new_size;
}

kresult_t Private_Normal_Region::clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                          size_t base_addr, u64 new_access)
{
    auto copy = klib::make_unique<Private_Normal_Region>(*this);
    if (!copy)
        return -ENOMEM;

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    auto result = owner->copy_pages(new_table, start_addr, base_addr, size, new_access);
    if (result)
        return result;

    new_table->paging_regions.insert(copy.release());
    return 0;
}

void Private_Normal_Region::trim(u64 new_start, u64 new_size) noexcept
{
    assert(new_start >= start_addr);

    start_addr = new_start;
    size       = new_size;
}

void Mem_Object_Reference::trim(u64 new_start, u64 new_size) noexcept
{
    assert(new_start >= start_addr);

    if (new_start != start_addr) {
        u64 diff = new_start - start_addr;

        if (diff <= start_offset_bytes) {
            start_offset_bytes -= diff;
            object_offset_bytes += diff;
            object_size_bytes = object_size_bytes < diff ? 0 : object_size_bytes - diff;
        } else {
            // Round down to the page boundary
            u64 t = start_offset_bytes & 0xfff;
            start_offset_bytes -= t;
            object_offset_bytes -= t;
            object_size_bytes += t;

            start_offset_bytes = start_offset_bytes < diff ? 0 : start_offset_bytes - diff;
            object_offset_bytes += diff;
            object_size_bytes = object_size_bytes < diff ? 0 : object_size_bytes - diff;
        }
    }

    size = new_size;
    if (size < start_offset_bytes + object_size_bytes)
        object_size_bytes = size - start_offset_bytes;
}

kresult_t Private_Normal_Region::punch_hole(u64 hole_addr_start, u64 hole_size_bytes)
{
    assert(start_addr < hole_addr_start and start_addr + size > hole_addr_start + hole_size_bytes);

    auto ptr = new Private_Normal_Region(*this);
    if (!ptr)
        return -ENOMEM;

    ptr->size -= hole_addr_start + hole_size_bytes - start_addr;
    ptr->start_addr = hole_addr_start + hole_size_bytes;
    owner->paging_regions.insert(ptr);
    size = hole_addr_start - start_addr;

    return 0;
}

kresult_t Phys_Mapped_Region::punch_hole(u64 hole_addr_start, u64 hole_size_bytes)
{
    assert(start_addr < hole_addr_start and start_addr + size > hole_addr_start + hole_size_bytes);

    auto ptr = new Phys_Mapped_Region(*this);
    if (!ptr)
        return -ENOMEM;

    ptr->size -= hole_addr_start + hole_size_bytes - start_addr;
    ptr->start_addr = hole_addr_start + hole_size_bytes;
    ptr->phys_addr_start += ptr->start_addr - start_addr;
    owner->paging_regions.insert(ptr);
    size = hole_addr_start - start_addr;

    return 0;
}

kresult_t Mem_Object_Reference::punch_hole(u64 hole_addr_start, u64 hole_size_bytes)
{
    assert(start_addr < hole_addr_start and start_addr + size > hole_addr_start + hole_size_bytes);

    const u64 new_start = hole_addr_start + hole_size_bytes;

    auto ptr = new Mem_Object_Reference(*this);
    if (!ptr)
        return -ENOMEM;

    ptr->trim(new_start, size - (new_start - start_addr));
    owner->paging_regions.insert(ptr);
    auto it = owner->object_regions.find(references.get());
    assert(it != owner->object_regions.end());
    it->second.insert(ptr);

    trim(start_addr, hole_addr_start - start_addr);

    return 0;
}

Page_Table_Argumments Mem_Object_Reference::craft_arguments() const
{
    return {
        !!(access_type & Readable),
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable),
        0b010,
    };
}

ReturnStr<bool> Mem_Object_Reference::alloc_page(u64 ptr_addr)
{
    if (cow) {
        const auto reg_addr = (ptr_addr & ~0xfffUL) - start_addr;

        if (reg_addr + 0x1000 <= start_offset_bytes or
            reg_addr >= start_offset_bytes + object_size_bytes) {
            // Out of object range. Allocate an empty page
            auto page = pmm::Page_Descriptor::allocate_page_zeroed(12);
            if (!page.success())
                return page.propagate();

            auto res = owner->map(klib::move(page.val), ptr_addr, craft_arguments());
            if (res)
                return Error(res);

            return true;
        }

        auto page =
            references->atomic_request_page(reg_addr - start_offset_bytes + object_offset_bytes);
        if (!page.success())
            return page.propagate();

        if (not page.val.page_struct_ptr)
            return false;

        page = page.val.create_copy();
        if (!page.success())
            return page.propagate();

        // Partially zero the page, if needed

        if (reg_addr >= start_offset_bytes and
            reg_addr + 0x1000 <= start_offset_bytes + object_size_bytes) {
            // Whole page; just map it
            auto result = owner->map(klib::move(page.val), ptr_addr, craft_arguments());
            if (result)
                return Error(result);

            return true;
        }

        // Zero the page
        Temp_Mapper_Obj<void> mapper(request_temp_mapper());
        void *pageptr = mapper.map(page.val.page_struct_ptr->get_phys_addr());

        // Zero the start of the page
        const u64 start_offset_size =
            reg_addr < start_offset_bytes ? start_offset_bytes - reg_addr : 0;
        memset((char *)pageptr, 0, start_offset_size);

        // Zero the end of the page
        const u64 obj_limit = start_offset_bytes + object_size_bytes;
        const u64 end_offset_size =
            reg_addr + 0x1000 > obj_limit ? reg_addr + 0x1000 - obj_limit : 0;
        const u64 end_offset_start = 0x1000 - end_offset_size;
        memset((char *)pageptr + end_offset_start, 0, end_offset_size);

        auto result = owner->map(klib::move(page.val), ptr_addr, craft_arguments());
        if (result)
            return Error(result);

        return true;
    } else {
        // Find the actual address of the page inside the object
        const auto addr_aligned = ptr_addr & ~0xfffUL;
        const auto reg_addr     = addr_aligned - start_addr + object_offset_bytes;
        auto page               = references->atomic_request_page(reg_addr);
        if (!page.success())
            return page.propagate();

        if (not page.val.page_struct_ptr)
            return false;

        auto result = owner->map(klib::move(page.val), addr_aligned, craft_arguments());
        if (result)
            return Error(result);

        return true;
    }
}

kresult_t Mem_Object_Reference::move_to(TLBShootdownContext &ctx, const klib::shared_ptr<Page_Table> &new_table,
                                        u64 base_addr, u64 new_access)
{
    return -ENOSYS;
    // throw Kern_Exception(-ENOSYS, "move_to of Mem_Object_Reference was not yet implemented");
}

kresult_t Mem_Object_Reference::clone_to(const klib::shared_ptr<Page_Table> &new_table,
                                         u64 base_addr, u64 new_access)
{
    auto copy = klib::make_unique<Mem_Object_Reference>(*this);
    if (!copy)
        return -ENOMEM;

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    if (cow) {
        auto result = owner->copy_pages(new_table, start_addr, base_addr, size, new_access);
        if (result)
            return result;
    }

    auto it = new_table->object_regions.find(copy->references.get());
    if (it == new_table->object_regions.end()) {
        auto result = new_table->object_regions.insert({copy->references.get(), {}});
        if (result.first == new_table->object_regions.end())
            return -ENOMEM;

        it = result.first;
    }
    it->second.insert(copy.get());

    new_table->paging_regions.insert(copy.release());
    return 0;
}

void Generic_Mem_Region::prepare_deletion() noexcept
{
    // Do nothing
}

void Mem_Object_Reference::prepare_deletion() noexcept
{
    auto p = owner->object_regions.find(references.get());
    if (p != owner->object_regions.end())
        p->second.erase(this);
    
    if (p->second.empty())
        owner->object_regions.erase(p);
}

Mem_Object_Reference::Mem_Object_Reference(u64 start_addr, u64 size, klib::string name,
                                           Page_Table *owner, u8 access,
                                           klib::shared_ptr<Mem_Object> references,
                                           u64 object_offset_bytes, bool copy_on_write,
                                           u64 start_offset_bytes, u64 object_size_bytes)
    : Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access),
      references(klib::forward<klib::shared_ptr<Mem_Object>>(references)),
      start_offset_bytes(start_offset_bytes), object_offset_bytes(object_offset_bytes),
      object_size_bytes(object_size_bytes), cow(copy_on_write)
{
    assert(cow or (start_offset_bytes == 0) or !"non-CoW region cannot have start offset");
    assert(cow or (object_size_bytes == size) or
           !"non-CoW region cannot have size different from the object size");
    assert((object_offset_bytes & 0xfff) == (start_offset_bytes & 0xfff) or
           !"Object page-misaligned with region");
}

void Generic_Mem_Region::rcu_free() noexcept
{
    RCU_Head &rcu = rcu_head;
    rcu.rcu_func  = rcu_callback;
    rcu.rcu_next  = nullptr;

    get_cpu_struct()->paging_rcu_cpu.push(&rcu);
}

void Generic_Mem_Region::rcu_callback(void *ptr, bool)
{
    auto region = reinterpret_cast<Generic_Mem_Region *>((char *)ptr -
                                                         offsetof(Generic_Mem_Region, rcu_head));

    delete region;
}