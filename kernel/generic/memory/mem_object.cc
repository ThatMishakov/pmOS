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

#include "mem_object.hh"

#include "paging.hh"
#include "pmm.hh"
#include "temp_mapper.hh"
#include "vmm.hh"

#include <assert.h>
#include <exceptions.hh>
#include <messaging/messaging.hh>
#include <pmos/ipc.h>
#include <pmos/utility/scope_guard.hh>

using namespace kernel;

Mem_Object::Mem_Object(u64 page_size_log, u64 size_pages, u32 max_user_permissions)
    : page_size_log(page_size_log), pages_storage(nullptr), pages_size(size_pages),
      max_user_access_perm(max_user_permissions) {};

Mem_Object::~Mem_Object()
{
    atomic_erase_gloabl_storage(id);

    auto current_page = pages_storage;
    while (current_page) {
        auto p       = current_page;
        current_page = current_page->l.next;

        // This should RAII free the page
        pmm::Page_Descriptor::from_raw_ptr(p);
    }
}

klib::shared_ptr<Mem_Object> Mem_Object::create(u64 page_size_log, u64 size_pages)
{
    // Create new object
    klib::shared_ptr<Mem_Object> ptr(
        new Mem_Object(page_size_log, size_pages,
                       Protection::Readable | Protection::Writeable | Protection::Executable));
    if (!ptr)
        return nullptr;

    // Atomically insert into the object storage
    auto e = atomic_push_global_storage(ptr);
    if (e)
        return nullptr;

    return ptr;
}

klib::shared_ptr<Mem_Object> Mem_Object::create_from_phys(u64 phys_addr, u64 size_bytes,
                                                          bool take_ownership,
                                                          u32 max_user_permissions)
{
    const u64 size_alligned  = (size_bytes + 0xFFF) & ~0xFFFUL;
    const u64 start_alligned = phys_addr & ~0xFFFUL;
    const u64 pages_count    = size_alligned >> 12;

    assert(take_ownership && "not taking ownership is not implemented");

    // Create new object
    klib::shared_ptr<Mem_Object> ptr(new Mem_Object(12, pages_count, max_user_permissions));
    if (!ptr)
        return nullptr;

    // Lock the object so nobody overwrites it while the pages are inserted
    Auto_Lock_Scope l(ptr->lock);

    // Atomically insert into the object storage
    auto e = atomic_push_global_storage(ptr);
    if (e)
        return nullptr;

    // Provide the pages
    // This can't fail
    for (u64 i = 0; i < pages_count; ++i) {
        auto page = pmm::Page_Descriptor::create_from_allocated(start_alligned + i * 0x1000);
        auto c    = page.page_struct_ptr;
        page.takeout_page();
        c->l.offset        = i * 0x1000;
        c->l.next          = ptr->pages_storage;
        ptr->pages_storage = c;
    }

    return ptr;
}

Mem_Object::id_type Mem_Object::get_id() const noexcept { return id; }

kresult_t Mem_Object::atomic_push_global_storage(klib::shared_ptr<Mem_Object> o)
{
    // RAII lock against concurrent changes to the map
    Auto_Lock_Scope l(object_storage_lock);

    auto id = o->get_id();

    auto p = objects_storage.insert({klib::move(id), klib::move(o)});
    if (p.first == objects_storage.end()) [[unlikely]]
        return -ENOMEM;

    if (!p.second) [[unlikely]]
        return -EEXIST;

    return 0;
}

void Mem_Object::atomic_erase_gloabl_storage(id_type object_to_delete)
{
    // RAII lock against concurrent changes to the map
    Auto_Lock_Scope l(object_storage_lock);

    objects_storage.erase(object_to_delete);
}

kresult_t Mem_Object::register_pined(klib::weak_ptr<Page_Table> pined_by)
{
    assert(pinned_lock.is_locked() && "lock is not locked!");

    auto t = this->pined_by.insert(pined_by);
    if (t.first == this->pined_by.end())
        return -ENOMEM;

    if (not t.second)
        return -EEXIST;

    return 0;
}

void Mem_Object::atomic_unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept
{
    Auto_Lock_Scope l(pinned_lock);
    return unregister_pined(klib::move(pined_by));
}

void Mem_Object::unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept
{
    assert(pinned_lock.is_locked() && "lock is not locked!");

    this->pined_by.erase(pined_by);
}

ReturnStr<pmm::Page_Descriptor> Mem_Object::atomic_request_page(u64 offset) noexcept
{
    Auto_Lock_Scope l(lock);
    return request_page(offset);
}

ReturnStr<pmm::Page_Descriptor> Mem_Object::request_page(u64 offset) noexcept
{
    const auto index = offset >> page_size_log;

    if (pages_size <= index)
        return Error(-EINVAL);

    offset &= ~0xfffUL;
    auto page = pages_storage;
    while (page and page->l.offset != offset)
        page = page->l.next;

    if (page) {
        if (page->has_physical_page()) {
            return pmm::Page_Descriptor::dup_from_raw_ptr(page);
        }

        return pmm::Page_Descriptor::none();
    } else {
        // TODO
        auto pager_port = pager;
        if (not pager_port) {
            auto pp = pmm::Page_Descriptor::allocate_page(page_size_log);
            if (!pp.success())
                return pp.propagate();

            auto &p                     = pp.val;
            auto p2                     = p.duplicate();
            p.page_struct_ptr->l.offset = offset;
            p.page_struct_ptr->l.next   = pages_storage;
            pages_storage               = p.page_struct_ptr;
            p.takeout_page();
            return klib::move(p2);
        }

        auto p = pmm::Page_Descriptor::create_empty();
        if (!p.page_struct_ptr)
            return Error(-ENOMEM);

        IPC_Kernel_Request_Page request {
            .type          = IPC_Kernel_Request_Page_NUM,
            .flags         = 0,
            .mem_object_id = id,
            .page_offset   = offset,
        };
        auto result = pager_port->atomic_send_from_system(reinterpret_cast<char *>(&request), sizeof(request));
        if (result)
            return Error(result);

        p.page_struct_ptr->l.offset = offset;
        p.page_struct_ptr->l.next   = pages_storage;
        pages_storage               = p.page_struct_ptr;
        p.takeout_page();
        return pmm::Page_Descriptor::none();
    }

    assert(false);
}

kresult_t Mem_Object::atomic_resize(u64 new_size_pages)
{
    Auto_Lock_Scope resize_l(resize_lock);

    u64 old_size;

    {
        Auto_Lock_Scope l(lock);

        // Firstly, change the pages_size before resizing the vector. This is needed because even
        // though the object has been shrunk, there might still be regions referencing pages in it,
        // so notify page tables of the change before shrinking the vector and releasing pages to
        // stop people from writing to the pages that were freed.
        old_size   = pages_size;
        pages_size = new_size_pages;

        // If the new size is larger, there is no need to shrink memory regions
        if (old_size <= new_size_pages) {
            return 0;
        }
    }

    const auto self           = shared_from_this();
    const auto new_size_bytes = new_size_pages << page_size_log;

    {
        Auto_Lock_Scope l(pinned_lock);
        for (const auto &a: pined_by) {
            const auto ptr = a.lock();
            if (ptr)
                ptr->atomic_shrink_regions(self, new_size_bytes);
        }
    }

    pmm::Page *pages = nullptr;

    {
        Auto_Lock_Scope l(lock);

        pmm::Page **erase_ptr_head = &pages_storage;
        while (*erase_ptr_head) {
            pmm::Page *current = *erase_ptr_head;

            if (current->l.offset >= new_size_pages) {
                *erase_ptr_head = current->l.next;
                current->l.next = pages;
                pages           = current;
            } else {
                if (current->l.next == nullptr)
                    break;

                erase_ptr_head = &current->l.next;
            }
        }
    }

    while (pages) {
        pmm::Page *next = pages->l.next;
        pages           = next;
        pmm::Page_Descriptor::from_raw_ptr(pages);
    }

    return 0;
}

klib::shared_ptr<Mem_Object> Mem_Object::get_object(u64 object_id)
{
    Auto_Lock_Scope l(object_storage_lock);
    return objects_storage.get_copy_or_default(object_id).lock();
}

ReturnStr<bool> Mem_Object::read_to_kernel(u64 offset, void *buffer, u64 size)
{
    if (size == 0)
        return true;

    Auto_Lock_Scope l(lock);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (u64 i = offset & ~0xfffUL; i < offset + size; i += 0x1000) {
        const auto page = request_page(i);
        if (!page.success())
            return page.propagate();

        auto page_struct = page.val.page_struct_ptr;

        if (!page_struct)
            return false;

        char *ptr = mapper.map(page_struct->get_phys_addr());

        const u64 start = i < offset ? offset : i;
        const u64 end   = i + 0x1000 < offset + size ? i + 0x1000 : offset + size;
        const u64 len   = end - start;
        memcpy(reinterpret_cast<char *>(buffer) + start - offset, ptr + (start & 0xfff), len);
    }

    return true;
}

ReturnStr<void *> Mem_Object::map_to_kernel(u64 offset, u64 size, Page_Table_Argumments args)
{
    // Lock might be needed here?
    // Also, TODO: magic numbers everywhere
    u64 object_size_bytes = pages_size * 4096;

    assert((offset & 0xfff) == 0);
    assert(size > 0 && (size & 0xfff) == 0);
    assert(offset + size <= object_size_bytes);

    const size_t size_pages = size >> 12;

    // Make sure all pages are allocated
    for (size_t i = 0; i < size; i += 4096) {
        auto p = atomic_request_page(offset + i);
        if (!p.success())
            return p.propagate();

        if (not p.val.page_struct_ptr)
            return nullptr;
    }

    void *mem_virt = vmm::kernel_space_allocator.virtmem_alloc(size >> 12);
    if (mem_virt == nullptr)
        return Error(-ENOMEM);

    size_t i   = 0;
    auto guard = pmos::utility::make_scope_guard([&]() {
        for (size_t ii = 0; ii < i; ++ii) {
            void *const virt_addr = (void *)(size_t(mem_virt) + ii);
            unmap_kernel_page(virt_addr);
        }

        vmm::kernel_space_allocator.virtmem_free(mem_virt, size_pages);
    });

    for (i = 0; i < size; i += 4096) {
        auto p = atomic_request_page(offset + i);
        assert(p.success());

        assert(p.val.page_struct_ptr);
        Page_Table_Argumments arg = args;
        arg.extra |= PAGING_FLAG_STRUCT_PAGE;

        // TODO: This is weird business and doesn't do refcounting
        const u64 phys_addr   = p.val.page_struct_ptr->get_phys_addr();
        void *const virt_addr = (void *)(ulong(mem_virt) + i);
        auto result           = map_kernel_page(phys_addr, virt_addr, arg);
        if (result)
            return Error(result);
    }

    guard.dismiss();

    return mem_virt;
}

size_t Mem_Object::size_bytes() const noexcept { return pages_size << page_size_log; }