#include "mem_regions.hh"
#include "paging.hh"
#include <kernel/errors.h>
#include "mem.hh"
#include <pmos/ipc.h>
#include <processes/tasks.hh>
#include <assert.h>
#include "mem_object.hh"
#include "temp_mapper.hh"

u64 counter = 1;

bool Generic_Mem_Region::on_page_fault(u64 error, u64 pagefault_addr)
{
    if (protection_violation(error))
        throw(Kern_Exception(ERROR_PROTECTION_VIOLATION, "violation of protection policy"));

    if (not has_permissions(error))
        throw(Kern_Exception(ERROR_PROTECTION_VIOLATION, "task has no permission to do the operation"));

    if (owner->is_mapped(pagefault_addr)) {
        // Some CPUs supposedly remember invalid pages. INVALPG might be needed here...

        return true;
    }

    return alloc_page(pagefault_addr);
}

bool Generic_Mem_Region::prepare_page(u64 access_mode, u64 page_addr)
{
    if (not has_access(access_mode))
        throw (Kern_Exception(ERROR_OUT_OF_RANGE, "process has no access to the page"));

    if (owner->is_mapped(page_addr))
        return true;

    return alloc_page(page_addr);
}

Page_Table_Argumments Private_Normal_Region::craft_arguments() const
{
    return {
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable) ,
        0b000,
    };
}

bool Private_Normal_Region::alloc_page(u64 ptr_addr)
{
    void* new_page = kernel_pframe_allocator.alloc_page();

    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;

    try {
        owner->map((u64)new_page, page_addr, args);

        // TODO: This *will* explode if the page is read-only
        for (size_t i = 0; i < 4096/sizeof(u64); ++i) {
            ((u64 *)page_addr)[i] = pattern;
        }

        return true;
    } catch (...) {
        kernel_pframe_allocator.free(new_page);
        throw;
    }
}

Page_Table_Argumments Phys_Mapped_Region::craft_arguments() const
{
    return {
        !!(access_type & Readable),
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable) ,
        0b010,
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

void Generic_Mem_Region::move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    auto self = shared_from_this();

    new_table->paging_regions.insert({base_addr, self});

    try {
        Page_Table * const old_owner = owner;

        old_owner->move_pages(new_table, start_addr, base_addr, size, new_access);

        old_owner->paging_regions.erase(start_addr);

        owner = new_table.get();

        access_type = new_access;
        start_addr = base_addr;
    } catch (...) {
        new_table->paging_regions.erase(base_addr);
        throw;
    }
}

void Phys_Mapped_Region::clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    auto copy = klib::make_shared<Phys_Mapped_Region>(*this);

    copy->owner = new_table.get();
    copy->id = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr = base_addr;

    new_table->paging_regions.insert({base_addr, copy});
}

void Private_Normal_Region::clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    auto copy = klib::make_shared<Private_Normal_Region>(*this);

    copy->owner = new_table.get();
    copy->id = __atomic_add_fetch(&counter, 1, 0);
    copy->access_type = new_access;
    copy->start_addr = base_addr;

    new_table->paging_regions.insert({base_addr, copy});

    try {
        owner->copy_pages(new_table, start_addr, base_addr, size, new_access);
    } catch (...) {
        new_table->paging_regions.erase(base_addr);
        throw;
    }
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
        const auto reg_addr = (ptr_addr&~0xfffUL) - start_addr;

        if (reg_addr+0x1000 <= start_offset_bytes or reg_addr >= start_offset_bytes + object_size_bytes) {
            // Out of object range. Allocate an empty page
            auto page = Page_Descriptor::allocate_page(12);

            owner->map(klib::move(page), ptr_addr, craft_arguments());
            return true;
        }

        auto page = references->atomic_request_page(reg_addr - start_offset_bytes + object_offset_bytes);

        if (not page.available)
            return false;

        page = page.create_copy();

        // Partially zero the page, if needed

        if (reg_addr >= start_offset_bytes and reg_addr + 0x1000 <= start_offset_bytes + object_size_bytes) {
            // Whole page; just map it
            owner->map(klib::move(page), ptr_addr, craft_arguments());
            return true;
        }

        // Zero the page
        Temp_Mapper_Obj<void> mapper(request_temp_mapper());
        void *pageptr = mapper.map(page.page_ptr);

        // Zero the start of the page
        const u64 start_offset_size = reg_addr < start_offset_bytes ? start_offset_bytes - reg_addr : 0;
        memset((char*)pageptr, 0, start_offset_size);

        // Zero the end of the page
        const u64 obj_limit = start_offset_bytes + object_size_bytes;
        const u64 end_offset_size = reg_addr + 0x1000 > obj_limit ? reg_addr + 0x1000 - obj_limit : 0;
        const u64 end_offset_start = 0x1000 - end_offset_size;
        memset((char*)pageptr + end_offset_start, 0, end_offset_size);

        owner->map(klib::move(page), ptr_addr, craft_arguments());
        return true;
    } else {
        // Find the actual address of the page inside the object
        const auto reg_addr = ptr_addr - start_addr + start_offset_bytes;

        auto page = references->atomic_request_page(reg_addr);

        if (not page.available)
            return false;

        owner->map(klib::move(page), ptr_addr, craft_arguments());

        return true;
    }
}

void Mem_Object_Reference::move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    throw Kern_Exception(ERROR_NOT_IMPLEMENTED, "move_to of Mem_Object_Reference was not yet implemented");
}

void Mem_Object_Reference::clone_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    throw Kern_Exception(ERROR_NOT_IMPLEMENTED, "move_to of Mem_Object_Reference was not yet implemented");
}

void Generic_Mem_Region::prepare_deletion() noexcept
{
    // Do nothing
}

void Mem_Object_Reference::prepare_deletion() noexcept
{
    owner->unreference_object(references, this);
}

Mem_Object_Reference::Mem_Object_Reference(u64 start_addr, u64 size, klib::string name, Page_Table *owner, u8 access,
                         klib::shared_ptr<Mem_Object> references, u64 object_offset_bytes, bool copy_on_write, u64 start_offset_bytes, u64 object_size_bytes):
        Generic_Mem_Region(start_addr, size, klib::forward<klib::string>(name), owner, access), 
        references(klib::forward<klib::shared_ptr<Mem_Object>>(references)), start_offset_bytes(start_offset_bytes), 
        object_offset_bytes(object_offset_bytes), object_size_bytes(object_size_bytes), cow(copy_on_write)
        {
            if (not cow and start_offset_bytes != 0)
                throw Kern_Exception(ERROR_OUT_OF_RANGE, "non-CoW region cannot have start offset");

            if (not cow and object_size_bytes != size)
                throw Kern_Exception(ERROR_OUT_OF_RANGE, "non-CoW region cannot have size different from the object size");

            if ((object_offset_bytes&0xfff) != (start_offset_bytes&0xfff))
                throw Kern_Exception(ERROR_OUT_OF_RANGE, "Object page-misaligned with region");
        };