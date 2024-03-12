#include "mem_object.hh"
#include <assert.h>
#include <pmos/ipc.h>
#include <messaging/messaging.hh>
#include "mem.hh"
#include <exceptions.hh>
#include "paging.hh"
#include "temp_mapper.hh"

Mem_Object::Mem_Object(u64 page_size_log, u64 size_pages):
        page_size_log(page_size_log), pages(size_pages), pages_size(size_pages) {};

Mem_Object::~Mem_Object()
{
    atomic_erase_gloabl_storage(id);

    for (size_t i = 0; i < pages.size(); ++i)
        try_free_page(pages[i], page_size_log);
}

klib::shared_ptr<Mem_Object> Mem_Object::create(u64 page_size_log, u64 size_pages)
{
    // Create new object
    klib::shared_ptr<Mem_Object> ptr(new Mem_Object(page_size_log, size_pages));

    // Atomically insert into the object storage
    atomic_push_global_storage(ptr);

    return ptr;
}

klib::shared_ptr<Mem_Object> Mem_Object::create_from_phys(u64 phys_addr, u64 size_bytes, bool take_ownership)
{
    const u64 size_alligned = (size_bytes + 0xFFF) & ~0xFFFUL;
    const u64 start_alligned = phys_addr & ~0xFFFUL;
    const u64 pages_count = size_alligned >> 12;
    
    // Create new object
    klib::shared_ptr<Mem_Object> ptr(new Mem_Object(12, pages_count));

    // Lock the object so nobody overwrites it while the pages are inserted
    Auto_Lock_Scope l(ptr->lock);

    // Atomically insert into the object storage
    atomic_push_global_storage(ptr);

    // Provide the pages
    // This can't fail
    for (u64 i = 0; i < pages_count; ++i)
        ptr->pages[i] = {
            .present = true,
            .dont_delete = !take_ownership,
            .requested = false,
            // Shift right by 10 because of x86 reasons...
            // No further explanation will be given :)
            // (In retrospect, I think this is somewhat stupid and needs to be changed, but whatever for now...)
            .ppn = (start_alligned + (i << 12)) >> 10,
        };

    return ptr;
}

Mem_Object::id_type Mem_Object::get_id() const noexcept
{
    return id;
}

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
        throw Kern_Exception(ERROR_OUT_OF_RANGE, "Trying to access to memory out of the Page Descriptor's range");

    auto& p = pages[index];

    do {
        // Page is already present
        if (p.present)
            continue;

        const auto pager_port = pager.lock();
        // Page not present and there is no pager -> alocate zeroed page
        if (not pager_port) {
            p = allocate_page(page_size_log);
            continue;
        }

        // Page not present, there is a pager and the page was already requested once -> do not request the page again
        if (p.requested)
            continue;

        // Request a page
        IPC_Kernel_Request_Page request {
            .type = IPC_Kernel_Request_Page_NUM,
            .flags = 0,
            .mem_object_id = id,
            .page_offset = offset,
        };
        pager_port->atomic_send_from_system(reinterpret_cast<char*>(&request), sizeof(request));

        p.requested = true;
    } while (false);

    return Page_Descriptor(true, false, page_size_log, p.get_page());
}

Mem_Object::Page_Storage Mem_Object::allocate_page(u8 size_log)
{
    assert(size_log == 12 && "only 4K pages are supported");

    return Page_Storage::from_allocated(kernel_pframe_allocator.alloc_page());
}

void Mem_Object::atomic_resize(u64 new_size_pages)
{
    Auto_Lock_Scope resize_l(resize_lock);

    u64 old_size;


    {
        Auto_Lock_Scope l(lock);

        // Firstly, change the pages_size before resizing the vector. This is needed because even though the
        // object has been shrunk, there might still be regions referencing pages in it, so notify page tables
        // of the change before shrinking the vector and releasing pages to stop people from writing to the pages that were freed.
        old_size = pages_size;
        pages_size = new_size_pages;

        // If the new size is larger, there is no need to shrink memory regions
        if (old_size <= new_size_pages) {
            try {
                pages.resize(new_size_pages);
            } catch (...) {
                pages_size = old_size;
                throw;
            }
            return;
        }
    }

    // This should never throw... but who knows
    try {
        const auto self = shared_from_this();
        const auto new_size_bytes = new_size_pages << page_size_log;

        {
            Auto_Lock_Scope l(pinned_lock);
            for (const auto &a : pined_by) {
                const auto ptr = a.lock();
                if (ptr)
                    ptr->atomic_shrink_regions(self, new_size_bytes);
            }
        }

        {
            Auto_Lock_Scope l(lock);

            for (size_t i = new_size_pages; i < old_size; ++i)
                try_free_page(pages[i], page_size_log);

            pages.resize(new_size_pages);
        }
    } catch (...) {
        Auto_Lock_Scope l(lock);
        pages_size = old_size;
        throw;
    }
}

klib::shared_ptr<Mem_Object> Mem_Object::get_object(u64 object_id)
{
    Auto_Lock_Scope l(object_storage_lock);

    auto p = objects_storage.get_copy_or_default(object_id).lock();

    if (not p)
        throw Kern_Exception(ERROR_OBJECT_DOESNT_EXIST, "memory object not found");

    return p;
}

void Mem_Object::try_free_page(Page_Storage &p, u8 page_size_log) noexcept
{
    assert(page_size_log == 12 && "Only 4K pages are supported");

    if (p.present and not p.dont_delete) {
        kernel_pframe_allocator.free(reinterpret_cast<void*>(p.get_page()));
        p = Page_Storage();
    }
}

bool Mem_Object::read_to_kernel(u64 offset, void *buffer, u64 size)
{
    if (size == 0)
        return true;

    Auto_Lock_Scope l(lock);

    Temp_Mapper_Obj<char> mapper(request_temp_mapper());
    for (u64 i = offset&~0xfffUL; i < offset+size; i += 0x1000) {
        const auto page = request_page(i);
        if (not page.available)
            return false;

        char * ptr = mapper.map(page.page_ptr);

        const u64 start = i < offset ? offset : i;
        const u64 end = i + 0x1000 < offset + size ? i + 0x1000 : offset + size;
        const u64 len = end - start;
        memcpy(reinterpret_cast<char*>(buffer) + start - offset, ptr + (start & 0xfff), len);
    }

    return true;
}