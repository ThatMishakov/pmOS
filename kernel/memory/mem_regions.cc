#include "mem_regions.hh"
#include "paging.hh"
#include <kernel/errors.h>
#include "mem.hh"
#include <pmos/ipc.h>
#include <processes/tasks.hh>

u64 counter = 1;

bool Generic_Mem_Region::on_page_fault(u64 error, u64 pagefault_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    if (protection_violation(error))
        throw(Kern_Exception(ERROR_PROTECTION_VIOLATION, "violation of protection policy"));

    if (not has_permissions(error))
        throw(Kern_Exception(ERROR_PROTECTION_VIOLATION, "task has no permission to do the operation"));

    if (task->page_table->is_allocated(pagefault_addr))
        return true;

    return alloc_page(pagefault_addr, task);
}

bool Generic_Mem_Region::prepare_page(u64 access_mode, u64 page_addr, const klib::shared_ptr<TaskDescriptor>& task)
{
    if (not has_access(access_mode))
        throw (Kern_Exception(ERROR_OUT_OF_RANGE, "process has no access to the page"));

    if (task->page_table->is_allocated(page_addr))
        return true;

    return alloc_page(page_addr, task);
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

bool Private_Normal_Region::alloc_page(u64 ptr_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    void* new_page = kernel_pframe_allocator.alloc_page();

    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;

    try {
        owner.lock()->map((u64)new_page, page_addr, args);

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
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable) ,
        0b010,
    };
}

bool Phys_Mapped_Region::alloc_page(u64 ptr_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;
    u64 phys_addr = page_addr - start_addr + phys_addr_start;

    owner.lock()->map(phys_addr, page_addr, args);

    return true;
}

Page_Table_Argumments Private_Managed_Region::craft_arguments() const
{
    return {
        !!(access_type & Writeable),
        true,
        false,
        !(access_type & Executable) ,
        0b010,
    };
}

bool Private_Managed_Region::alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task)
{
    u64 page_addr = (u64)ptr_addr & ~07777UL;

    auto r = owner.lock()->check_if_allocated_and_set_flag(page_addr, 0b100, craft_arguments());

    // Other thread has already allocated the page
    if (r.allocated)
        return true;

    // Region hit for the first time. Otherwise, be nice and don't send messages to the userspace again
    if (not (r.prev_flags & 0b100)) {
        IPC_Kernel_Alloc_Page str {
            IPC_Kernel_Alloc_Page_NUM,
            0,
            task->page_table->id,
            page_addr,
        };

        klib::shared_ptr<Port> p = notifications_port.lock();
        if (not p)
            throw Kern_Exception(ERROR_PORT_DOESNT_EXIST, "Private_Managed_Region::alloc_page port doesn't exist");

        p->atomic_send_from_system((const char *)&str, sizeof(IPC_Kernel_Alloc_Page));
    }

    task->atomic_block_by_page(page_addr);

    return false;
}

void Generic_Mem_Region::move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    auto self = shared_from_this();

    new_table->paging_regions.insert({base_addr, self});

    try {
        klib::shared_ptr<Page_Table> old_owner = owner.lock();

        old_owner->move_pages(new_table, start_addr, base_addr, size, new_access);

        old_owner->paging_regions.erase(start_addr);

        owner = new_table;

        access_type = new_access;
        start_addr = base_addr;
    } catch (...) {
        new_table->paging_regions.erase(base_addr);
        throw;
    }
}

void Private_Managed_Region::move_to(const klib::shared_ptr<Page_Table>& new_table, u64 base_addr, u64 new_access)
{
    throw Kern_Exception(ERROR_NOT_SUPPORTED, "move_to of Private_Managed_Region is currently not supported");
}