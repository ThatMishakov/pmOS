#include "paging.hh"
#include <kernel/memory.h>
#include "mem.hh"
#include <kernel/errors.h>
#include <utils.hh>
#include <utils.hh>
#include <types.hh>
#include <kernel/com.h>
#include <lib/memory.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <kern_logger/kern_logger.hh>
#include "temp_mapper.hh"
#include "assert.h"

Page_Table::~Page_Table()
{
    for (const auto &i : paging_regions)
        i.second->prepare_deletion();

    paging_regions.clear();
    takeout_global_page_tables();

    for (const auto &p : mem_objects)
        p.first->atomic_unregister_pined(weak_from_this());
}

u64 Page_Table::atomic_create_normal_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 pattern)
{
    Auto_Lock_Scope scope_lock(lock);

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    paging_regions.insert({start_addr,
        klib::make_shared<Private_Normal_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), this, access, pattern
        )});

    return start_addr;
}

u64 Page_Table::atomic_transfer_region(const klib::shared_ptr<Page_Table>& to, u64 region_orig, u64 prefered_to, u64 access, bool fixed)
{
    Auto_Lock_Scope_Double scope_lock(lock, to->lock);

    try {
        auto& reg = paging_regions.at(region_orig);

        if (prefered_to&07777)
            prefered_to = 0;

        u64 start_addr = to->find_region_spot(prefered_to, reg->size, fixed);
        reg->move_to(to, start_addr, access);
        return start_addr;
    } catch (std::out_of_range& r) {
        throw Kern_Exception(ERROR_OUT_OF_RANGE, "atomic_transfer_region source not found");
    }
}

u64 Page_Table::atomic_create_phys_region(u64 page_aligned_start, u64 page_aligned_size, unsigned access, bool fixed, klib::string name, u64 phys_addr_start)
{
    Auto_Lock_Scope scope_lock(lock);

    if (phys_addr_start >= phys_addr_limit() or phys_addr_start + page_aligned_size >= phys_addr_limit() or phys_addr_start > phys_addr_start+page_aligned_size)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "atomic_create_phys_region phys_addr outside the supported by x86"));

    u64 start_addr = find_region_spot(page_aligned_start, page_aligned_size, fixed);

    paging_regions.insert({start_addr,
        klib::make_shared<Phys_Mapped_Region>(
            start_addr, page_aligned_size, klib::forward<klib::string>(name), this, access, phys_addr_start
        )});

    return start_addr;
}

Page_Table::pagind_regions_map::iterator Page_Table::get_region(u64 page)
{
    auto it = paging_regions.get_smaller_or_equal(page);
    if (it == paging_regions.end() or not it->second->is_in_range(page))
        return paging_regions.end();

    return it;
}

bool Page_Table::can_takeout_page(u64 page_addr) noexcept
{
    auto it = paging_regions.get_smaller_or_equal(page_addr);
    if (it == paging_regions.end() or not it->second->is_in_range(page_addr))
        return false;

    return it->second->can_takeout_page();
}

u64 Page_Table::find_region_spot(u64 desired_start, u64 size, bool fixed)
{
    u64 end = desired_start + size;

    bool region_ok = desired_start != 0;

    if (region_ok) {
        auto it = paging_regions.get_smaller_or_equal(desired_start);
        if (it != paging_regions.end() and it->second->is_in_range(desired_start))
            region_ok = false;
    }

    if (region_ok) {
        auto it = paging_regions.lower_bound(desired_start);
        if (it != paging_regions.end() and it->first < end)
            region_ok = false;
    }

    // If the new region doesn't overlap with anything, just use it
    if (region_ok) {
        return desired_start;

    } else {
        if (fixed)
            throw(Kern_Exception(ERROR_REGION_OCCUPIED, "find_region_spot fixed region overlaps"));

        u64 addr = 0x1000;
        auto it = paging_regions.begin();
        while (it != paging_regions.end()) {
            u64 end = addr + size;

            if (it->first > end and end <= user_addr_max())
                return addr;

            addr = it->second->addr_end();
            ++it;
        }

        if (addr + size > user_addr_max())
            throw(Kern_Exception(ERROR_NO_FREE_REGION, "find_region_spot no free region found"));

        return addr;
    }
}

bool Page_Table::prepare_user_page(u64 virt_addr, unsigned access_type, const klib::shared_ptr<TaskDescriptor>& task)
{
    auto it = paging_regions.get_smaller_or_equal(virt_addr);

    if (it == paging_regions.end() or not it->second->is_in_range(virt_addr))
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "user provided parameter is unallocated"));

    Generic_Mem_Region &reg = *it->second;

    const auto available = reg.prepare_page(access_type, virt_addr);
    if (not available)
        task->atomic_block_by_page(virt_addr, &blocked_tasks);

    return available;
}

void Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase_if_exists(this->id);
}

void Page_Table::insert_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.insert({id, weak_from_this()});
}

Page_Table::page_table_map Page_Table::global_page_tables;
Spinlock Page_Table::page_table_index_lock;

void Page_Table::unblock_tasks(u64 page)
{
    for (const auto& it : owner_tasks)
        it.lock()->atomic_try_unblock_by_page(page);
}

klib::shared_ptr<Page_Table> Page_Table::get_page_table(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    return global_page_tables.get_copy_or_default(id).lock();
}

klib::shared_ptr<Page_Table> Page_Table::get_page_table_throw(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    try {
        return global_page_tables.at(id).lock();
    } catch (const std::out_of_range&) {
        throw Kern_Exception(ERROR_OBJECT_DOESNT_EXIST, "requested page table doesn't exist");
    }
}

void Page_Table::map(u64 page_addr, u64 virt_addr)
{
    auto it = get_region(virt_addr);
    if (it == paging_regions.end())
        throw(Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "map no region found"));

    map(page_addr, virt_addr, it->second->craft_arguments());
}

u64 Page_Table::phys_addr_limit()
{
    // TODO: This is arch-specific
    return 0x01UL << 48;
}

void Page_Table::move_pages(const klib::shared_ptr<Page_Table>& to, u64 from_addr, u64 to_addr, u64 size_bytes, u8 access)
{
    u64 offset = 0;

    try {
        for (; offset < size_bytes; offset += 4096) {
            auto info = get_page_mapping(from_addr + offset);
            if (info.is_allocated) {
                Page_Table_Argumments arg = {
                    !!(access & Writeable),
                    info.user_access,
                    0,
                    not (access & Executable),
                    info.flags,
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

void Page_Table::copy_pages(const klib::shared_ptr<Page_Table>& to, u64 from_addr, u64 to_addr, u64 size_bytes, u8 access)
{
    u64 offset = 0;

        try {
        for (; offset < size_bytes; offset += 4096) {
            auto info = get_page_mapping(from_addr + offset);
            if (info.is_allocated) {
                Page_Table_Argumments arg = {
                    !!(access & Writeable),
                    info.user_access,
                    0,
                    not (access & Executable),
                    info.flags,
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
        throw Kern_Exception(ERROR_PAGE_PRESENT, "memory object is already referenced");

    try {
        Auto_Lock_Scope object_lock(object->pinned_lock);
        object->register_pined(weak_from_this());
    } catch (...) {
        mem_objects.erase(object);
        throw;
    }
}

void Page_Table::atomic_shrink_regions(const klib::shared_ptr<Mem_Object> &id, u64 new_size) noexcept
{
    Auto_Lock_Scope l(lock);

    auto p = mem_objects.at(id);
    auto it = p.regions.begin();
    while (it != p.regions.end()) {
        const auto &reg = *it;
        ++it;

    
        const auto object_end = reg->object_up_to();
        if (object_end < new_size) {
            const auto change_size_to = new_size > reg->start_offset_bytes ? new_size - reg->start_offset_bytes : 0UL;

            const auto free_from = reg->start_addr + change_size_to;
            const auto free_size = reg->addr_end() - free_from;

            if (change_size_to == 0) { // Delete region
                reg->prepare_deletion();
                paging_regions.erase(reg->start_addr);
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
        throw Kern_Exception(ERROR_PAGE_NOT_ALLOCATED, "memory region was not found");

    auto region_size = region->second->size;

    region->second->prepare_deletion();
    paging_regions.erase(region); 
    // paging_regions.erase(region_start);

    invalidate_range(region_start, region_size, true);
    unblock_tasks_rage(region_start, region_size);
}

void Page_Table::unreference_object(const klib::shared_ptr<Mem_Object> &object, Mem_Object_Reference *region) noexcept
{
    auto &p = mem_objects.at(object);

    p.regions.erase(region);
}

void Page_Table::unblock_tasks_rage(u64 blocked_by_page, u64 size_bytes)
{
    // TODO
}

Page_Descriptor Page_Table::Page_Info::create_copy() const
{
    if (not is_allocated)
        return {};

    if (nofree)
        // Return a reference
        return Page_Descriptor(true, false, page_addr, 12);

    // Return a copy
    Page_Descriptor new_page = Page_Descriptor::allocate_page(12);

    Temp_Mapper_Obj<char> this_mapping(request_temp_mapper());
    Temp_Mapper_Obj<char> new_mapping(request_temp_mapper());

    this_mapping.map(page_addr);
    new_mapping.map(new_page.page_ptr);

    const auto page_size_bytes = 4096;

    memcpy(new_mapping.ptr, this_mapping.ptr, page_size_bytes);


    return new_page;
}