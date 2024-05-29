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

#include "mem.hh"
#include "paging.hh"
#include "temp_mapper.hh"
#include "virtmem.hh"

#include <assert.h>
#include <exceptions.hh>
#include <messaging/messaging.hh>
#include <pmos/ipc.h>

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
        Page_Descriptor::from_raw_ptr(p);
    }
}

klib::shared_ptr<Mem_Object> Mem_Object::create(u64 page_size_log, u64 size_pages)
{
    // Create new object
    klib::shared_ptr<Mem_Object> ptr(
        new Mem_Object(page_size_log, size_pages,
                       Protection::Readable | Protection::Writeable | Protection::Executable));

    // Atomically insert into the object storage
    atomic_push_global_storage(ptr);

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

    // Lock the object so nobody overwrites it while the pages are inserted
    Auto_Lock_Scope l(ptr->lock);

    // Atomically insert into the object storage
    atomic_push_global_storage(ptr);

    // Provide the pages
    // This can't fail
    for (u64 i = 0; i < pages_count; ++i) {
        auto page = Page_Descriptor::create_from_allocated(start_alligned + i * 0x1000);
        auto c    = page.page_struct_ptr;
        page.takeout_page();
        c->l.offset        = i * 0x1000;
        c->l.next          = ptr->pages_storage;
        ptr->pages_storage = c;
    }

    return ptr;
}

Mem_Object::id_type Mem_Object::get_id() const noexcept { return id; }

void Mem_Object::atomic_push_global_storage(klib::shared_ptr<Mem_Object> o)
{
    // RAII lock against concurrent changes to the map
    Auto_Lock_Scope l(object_storage_lock);

    auto id = o->get_id();

    objects_storage.insert({klib::move(id), klib::move(o)});
}

void Mem_Object::atomic_erase_gloabl_storage(id_type object_to_delete)
{
    // RAII lock against concurrent changes to the map
    Auto_Lock_Scope l(object_storage_lock);

    objects_storage.erase(object_to_delete);
}

void Mem_Object::register_pined(klib::weak_ptr<Page_Table> pined_by)
{
    assert(pinned_lock.is_locked() && "lock is not locked!");

    this->pined_by.insert(pined_by);
}

void Mem_Object::atomic_unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept
{
    Auto_Lock_Scope l(pinned_lock);
    unregister_pined(klib::move(pined_by));
}

void Mem_Object::unregister_pined(const klib::weak_ptr<Page_Table> &pined_by) noexcept
{
    assert(pinned_lock.is_locked() && "lock is not locked!");

    this->pined_by.erase(pined_by);
}

Page_Descriptor Mem_Object::atomic_request_page(u64 offset)
{
    Auto_Lock_Scope l(lock);
    return request_page(offset);
}

Page_Descriptor Mem_Object::request_page(u64 offset)
{
    const auto index = offset >> page_size_log;

    if (pages_size <= index)
        throw Kern_Exception(-EINVAL,
                             "Trying to access to memory out of the Page Descriptor's range");

    offset &= ~0xfffUL;
    auto page = pages_storage;
    while (page and page->l.offset != offset)
        page = page->l.next;

    if (page) {
        if (page->has_physical_page()) {
            return Page_Descriptor::dup_from_raw_ptr(page);
        }

        return Page_Descriptor::none();
    } else {
        auto pager_port = pager.lock();
        if (not pager_port) {
            auto p                      = Page_Descriptor::allocate_page(page_size_log);
            auto p2                     = p.duplicate();
            p.page_struct_ptr->l.offset = offset;
            p.page_struct_ptr->l.next   = pages_storage;
            pages_storage               = p.page_struct_ptr;
            p.takeout_page();
            return klib::move(p2);
        }

        auto p = Page_Descriptor::create_empty();

        IPC_Kernel_Request_Page request {
            .type          = IPC_Kernel_Request_Page_NUM,
            .flags         = 0,
            .mem_object_id = id,
            .page_offset   = offset,
        };
        pager_port->atomic_send_from_system(reinterpret_cast<char *>(&request), sizeof(request));

        p.page_struct_ptr->l.offset = offset;
        p.page_struct_ptr->l.next   = pages_storage;
        pages_storage               = p.page_struct_ptr;
        p.takeout_page();
        return Page_Descriptor::none();
    }

    assert(false);
}

void Mem_Object::atomic_resize(u64 new_size_pages)
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
            return;
        }
    }

    // This should never throw... but who knows
    try {
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
    } catch (...) {
        Auto_Lock_Scope l(lock);
        pages_size = old_size;
        throw;
    }

    Page *pages = nullptr;

    {
        Auto_Lock_Scope l(lock);

        Page **erase_ptr_head = &pages_storage;
        while (*erase_ptr_head) {
            Page *current = *erase_ptr_head;

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
        Page *next = pages->l.next;
        pages      = next;
        Page_Descriptor::from_raw_ptr(pages);
    }
}

klib::shared_ptr<Mem_Object> Mem_Object::get_object(u64 object_id)
{
    Auto_Lock_Scope l(object_storage_lock);

    auto p = objects_storage.get_copy_or_default(object_id).lock();

    if (not p)
        throw Kern_Exception(-ENOENT, "memory object not found");

    return p;
}

bool Mem_Object::read_to_kernel(u64 offset, void *buffer, u64 size)
{
    if (size == 0)
        return true;

    Auto_Lock_Scope l(lock);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (u64 i = offset & ~0xfffUL; i < offset + size; i += 0x1000) {
        const auto page = request_page(i);
        if (not page.page_struct_ptr)
            return false;

        char *ptr = mapper.map(page.page_struct_ptr->page_ptr);

        const u64 start = i < offset ? offset : i;
        const u64 end   = i + 0x1000 < offset + size ? i + 0x1000 : offset + size;
        const u64 len   = end - start;
        memcpy(reinterpret_cast<char *>(buffer) + start - offset, ptr + (start & 0xfff), len);
    }

    return true;
}

void *Mem_Object::map_to_kernel(u64 offset, u64 size, Page_Table_Argumments args)
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
        if (not p.page_struct_ptr)
            return nullptr;
    }

    void *mem_virt = kernel_space_allocator.virtmem_alloc(size >> 12);
    if (mem_virt == nullptr)
        throw Kern_Exception(-ENOMEM, "could not get virtual memory for mapping an object");

    size_t i = 0;
    try {
        for (i = 0; i < size; i += 4096) {
            auto p = atomic_request_page(offset + i);
            assert(p.page_struct_ptr);
            Page_Table_Argumments arg = args;
            arg.extra |= PAGING_FLAG_STRUCT_PAGE;

            const u64 phys_addr   = p.page_struct_ptr->page_ptr;
            void *const virt_addr = (void *)(ulong(mem_virt) + i);
            map_kernel_page(phys_addr, virt_addr, arg);
        }
    } catch (...) {
        for (size_t ii = 0; ii < i; ++ii) {
            void *const virt_addr = (void *)(ulong(mem_virt) + ii);
            unmap_kernel_page(virt_addr);
        }

        kernel_space_allocator.virtmem_free(mem_virt, size_pages);
    }

    return mem_virt;
}

size_t Mem_Object::size_bytes() const noexcept { return pages_size << page_size_log; }