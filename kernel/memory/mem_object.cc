#include "mem_object.hh"

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