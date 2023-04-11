#include "mem_object.hh"
#include <assert.h>
#include <pmos/ipc.h>
#include <messaging/messaging.hh>
#include "mem.hh"

Mem_Object::Mem_Object(u64 page_size_log, u64 size_pages):
        page_size_log(page_size_log), pages(size_pages) {};

Mem_Object::~Mem_Object()
{
    atomic_erase_gloabl_storage(id);
}

klib::shared_ptr<Mem_Object> Mem_Object::create(u64 page_size_log, u64 size_pages)
{
    // Create new object
    klib::shared_ptr<Mem_Object> ptr(new Mem_Object(page_size_log, size_pages));

    // Atomically insert into the object storage
    atomic_push_global_storage(ptr);

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
    const auto index = offset >> page_size_log;

    Auto_Lock_Scope l(lock);
        
    assert(pages.size() > index && "Requesting page outside storage boundary");

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