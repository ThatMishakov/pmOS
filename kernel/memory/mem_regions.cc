#include "mem_regions.hh"
#include "paging.hh"
#include <kernel/errors.h>
#include "mem.hh"
#include <pmos/ipc.h>
#include <processes/tasks.hh>

u64 counter = 1;

Generic_Mem_Region::~Generic_Mem_Region()
{
    free_pages_range(start_addr, size, owner_cr3);
}

kresult_t Generic_Mem_Region::on_page_fault(u64 error, u64 pagefault_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    if (not is_in_range(pagefault_addr))
        return ERROR_OUT_OF_RANGE;

    if (protection_violation(error))
        return ERROR_PROTECTION_VIOLATION;

    if (not has_permissions(error))
        return ERROR_OUT_OF_RANGE;

    if (task->page_table->is_allocated(pagefault_addr))
        return SUCCESS;

    return alloc_page(pagefault_addr, task);
}

kresult_t Generic_Mem_Region::prepare_page(u64 access_mode, u64 page_addr, const klib::shared_ptr<TaskDescriptor>& task)
{
    if (not is_in_range(page_addr))
        return ERROR_OUT_OF_RANGE;

    if (not has_access(access_mode))
        return ERROR_OUT_OF_RANGE;

    if (task->page_table->is_allocated(page_addr))
        return SUCCESS;

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

kresult_t Private_Normal_Region::alloc_page(u64 ptr_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    auto new_page = kernel_pframe_allocator.alloc_page();
    if (new_page.result != SUCCESS)
        return new_page.result;

    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;

    kresult_t result = map((u64)new_page.val, page_addr, args);

    if (result != SUCCESS) {
        kernel_pframe_allocator.free(new_page.val);
    } else {
        for (size_t i = 0; i < 4096/sizeof(u64); ++i) {
            ((u64 *)page_addr)[i] = pattern;
        }
    }

    return result;
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

kresult_t Phys_Mapped_Region::alloc_page(u64 ptr_addr, [[maybe_unused]] const klib::shared_ptr<TaskDescriptor>& task)
{
    Page_Table_Argumments args = craft_arguments();

    u64 page_addr = (u64)ptr_addr & ~07777UL;
    u64 phys_addr = page_addr - start_addr + phys_addr_start;

    kresult_t result = map(phys_addr, page_addr, args);

    return result;
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

kresult_t Private_Managed_Region::alloc_page(u64 ptr_addr, const klib::shared_ptr<TaskDescriptor>& task)
{
    u64 page_addr = (u64)ptr_addr & ~07777UL;

    auto r = check_if_allocated_and_set_flag(page_addr, 0b100, craft_arguments());

    if (r.result != SUCCESS)
        return r.result;

    if (r.allocated)
        return SUCCESS;

    if (not (r.prev_flags & 0b100)) {
        IPC_Kernel_Alloc_Page str {
            IPC_Kernel_Alloc_Page_NUM,
            0,
            task->page_table->id,
            page_addr,
        };

        klib::shared_ptr<Port> p = notifications_port.lock();
        if (not p)
            return ERROR_PORT_DOESNT_EXIST;

        kresult_t result = p->atomic_send_from_system((const char *)&str, sizeof(IPC_Kernel_Alloc_Page));
        if (result != SUCCESS)
            return result;
    }

    kresult_t result = task->atomic_block_by_page(page_addr);

    return result == SUCCESS ? SUCCESS_REPEAT : result;
}