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

#include "mem.hh"
#include "mem_object.hh"
#include "paging.hh"
#include "temp_mapper.hh"

#include <assert.h>
#include <errno.h>
#include <kern_logger/kern_logger.hh>
#include <pmos/ipc.h>
#include <processes/tasks.hh>
#include <sched/sched.hh>
u64 counter = 1;

bool Generic_Mem_Region::on_page_fault(u64 access_type, u64 pagefault_addr)
{
    if (not has_access(access_type))
        throw Kern_Exception(-EFAULT, "task has no permission to do the operation");

    if (owner->is_mapped(pagefault_addr)) {
        // Some CPUs supposedly remember invalid pages.
        // RISC-V spec in particular says that unallocated pages can be cached
        owner->invalidate_tlb(pagefault_addr);
        return true;
    }

    return alloc_page(pagefault_addr);
}

bool Generic_Mem_Region::prepare_page(u64 access_mode, u64 page_addr)
{
    if (not has_access(access_mode))
        throw(Kern_Exception(-EFAULT, "process has no access to the page"));

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

bool Private_Normal_Region::alloc_page(u64 ptr_addr)
{
    auto page = Page_Descriptor::allocate_page(12);
    clear_page(page.page_struct_ptr->page_ptr, 0);
    owner->map(klib::move(page), ptr_addr, craft_arguments());
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

bool Phys_Mapped_Region::alloc_page(u64 ptr_addr)
{
    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;
    u64 phys_addr = page_addr - start_addr + phys_addr_start;

    owner->map(phys_addr, page_addr, args);

    return true;
}

void Generic_Mem_Region::move_to(const klib::shared_ptr<Page_Table> &new_table, u64 base_addr,
                                 u64 new_access)
{
    Page_Table *const old_owner = owner;

    old_owner->move_pages(new_table, start_addr, base_addr, size, new_access);

    old_owner->paging_regions.erase(this);
    new_table->paging_regions.insert(this);

    owner = new_table.get();

    access_type = new_access;
    start_addr  = base_addr;
}

void Phys_Mapped_Region::clone_to(const klib::shared_ptr<Page_Table> &new_table, u64 base_addr,
                                  u64 new_access)
{
    auto copy = new Phys_Mapped_Region(*this);

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    new_table->paging_regions.insert(copy);
}

void Private_Normal_Region::clone_to(const klib::shared_ptr<Page_Table> &new_table, u64 base_addr,
                                     u64 new_access)
{
    auto copy = klib::make_unique<Private_Normal_Region>(*this);

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    owner->copy_pages(new_table, start_addr, base_addr, size, new_access);
    new_table->paging_regions.insert(copy.release());
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

bool Mem_Object_Reference::alloc_page(u64 ptr_addr)
{
    if (cow) {
        const auto reg_addr = (ptr_addr & ~0xfffUL) - start_addr;

        if (reg_addr + 0x1000 <= start_offset_bytes or
            reg_addr >= start_offset_bytes + object_size_bytes) {
            // Out of object range. Allocate an empty page
            auto page = Page_Descriptor::allocate_page(12);

            clear_page(page.page_struct_ptr->page_ptr, 0);

            owner->map(klib::move(page), ptr_addr, craft_arguments());
            return true;
        }

        auto page =
            references->atomic_request_page(reg_addr - start_offset_bytes + object_offset_bytes);

        if (not page.page_struct_ptr)
            return false;

        page = page.create_copy();

        // Partially zero the page, if needed

        if (reg_addr >= start_offset_bytes and
            reg_addr + 0x1000 <= start_offset_bytes + object_size_bytes) {
            // Whole page; just map it
            owner->map(klib::move(page), ptr_addr, craft_arguments());
            return true;
        }

        // Zero the page
        Temp_Mapper_Obj<void> mapper(request_temp_mapper());
        void *pageptr = mapper.map(page.page_struct_ptr->page_ptr);

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

        owner->map(klib::move(page), ptr_addr, craft_arguments());
        return true;
    } else {
        // Find the actual address of the page inside the object
        const auto reg_addr = ptr_addr - start_addr + start_offset_bytes;

        auto page = references->atomic_request_page(reg_addr);

        if (not page.page_struct_ptr)
            return false;

        owner->map(klib::move(page), ptr_addr, craft_arguments());

        return true;
    }
}

void Mem_Object_Reference::move_to(const klib::shared_ptr<Page_Table> &new_table, u64 base_addr,
                                   u64 new_access)
{
    throw Kern_Exception(-ENOSYS, "move_to of Mem_Object_Reference was not yet implemented");
}

void Mem_Object_Reference::clone_to(const klib::shared_ptr<Page_Table> &new_table, u64 base_addr,
                                    u64 new_access)
{
    auto copy = klib::make_unique<Mem_Object_Reference>(*this);

    copy->owner       = new_table.get();
    copy->id          = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr  = base_addr;

    if (cow) {
        owner->copy_pages(new_table, start_addr, base_addr, size, new_access);
    }

    // TODO: This is problematic
    new_table->mem_objects[copy->references].regions.insert(copy.get());

    new_table->paging_regions.insert(copy.release());
}

void Generic_Mem_Region::prepare_deletion() noexcept
{
    // Do nothing
}

void Mem_Object_Reference::prepare_deletion() noexcept
{
    // owner->unreference_object(references, this);
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
    if (not cow and start_offset_bytes != 0)
        throw Kern_Exception(-EINVAL, "non-CoW region cannot have start offset");

    if (not cow and object_size_bytes != size)
        throw Kern_Exception(-EINVAL,
                             "non-CoW region cannot have size different from the object size");

    if ((object_offset_bytes & 0xfff) != (start_offset_bytes & 0xfff))
        throw Kern_Exception(-EINVAL, "Object page-misaligned with region");
};

void Generic_Mem_Region::rcu_free() noexcept
{
    RCU_Head &rcu = rcu_head;
    rcu.rcu_func  = rcu_callback;
    rcu.rcu_next  = nullptr;

    get_cpu_struct()->paging_rcu_cpu.push(&rcu);
}

void Generic_Mem_Region::rcu_callback(void *ptr)
{
    auto region = reinterpret_cast<Generic_Mem_Region *>((char *)ptr -
                                                         offsetof(Generic_Mem_Region, rcu_head));

    delete region;
}