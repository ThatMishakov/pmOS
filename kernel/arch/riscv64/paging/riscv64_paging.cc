#include "riscv64_paging.hh"
#include <memory/temp_mapper.hh>
#include <memory/mem.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>

void flush_page(void *virt) noexcept
{
    asm volatile("sfence.vma %0, x0" : : "r"(virt) : "memory");
}

kresult_t riscv_map_page(u64 pt_top_phys, u64 phys_addr, void * virt_addr, Page_Table_Argumments arg) noexcept
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_top_phys);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table

            if (entry->valid) {
                return ERROR_PAGE_PRESENT;
            }

            RISCV64_PTE new_entry = RISCV64_PTE();
            new_entry.valid = true;
            new_entry.readable = arg.readable;
            new_entry.writeable = arg.writeable;
            new_entry.executable = !arg.execution_disabled;
            new_entry.user = arg.user_access;
            new_entry.global = arg.global;
            new_entry.available = arg.extra;
            new_entry.ppn = phys_addr >> 12;

            active_pt[index] = new_entry;
            return SUCCESS;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                // Allocate a new page table
                u64 new_pt_phys;

                // TODO: Contemplate about removing exceptions :)
                try {
                    new_pt_phys = kernel_pframe_allocator.alloc_page_ppn();
                } catch (Kern_Exception &e) {
                    return e.err_code;
                }

                RISCV64_PTE new_entry = RISCV64_PTE();
                new_entry.valid = true;
                new_entry.ppn = new_pt_phys;
                *entry = new_entry;

                next_level_phys = new_pt_phys << 12;
                clear_page(next_level_phys);
            } else if (entry->is_leaf()) {
                return ERROR_HUGE_PAGE;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    // 0 paging levels???
    return ERROR_GENERAL;
}

kresult_t riscv_unmap_page(u64 pt_top_phys, void * virt_addr) noexcept
{
    // TODO: Return values of this function make no sense...

    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_top_phys);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table

            if (entry->valid) {
                return ERROR_PAGE_PRESENT;
            }

            RISCV64_PTE entry = active_pt[index];
            if (not entry.is_special()) {
                // Free the page
                kernel_pframe_allocator.free_ppn(entry.ppn);
            }
            entry = RISCV64_PTE();
            active_pt[index] = entry;
            flush_page(virt_addr);
            return SUCCESS;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                return ERROR_PAGE_NOT_PRESENT;
            } else if (entry->is_leaf()) {
                return ERROR_HUGE_PAGE;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    // 0 paging levels???
    return ERROR_GENERAL;
}

kresult_t map_page(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, Page_Table_Argumments arg)
{
    return riscv_map_page(page_table, phys_addr, (void*)virt_addr, arg);
}

kresult_t map_pages(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, u64 size, Page_Table_Argumments arg)
{
    // Don't overcomplicate things for now, just call map
    // This is *very* inefficient
    for (u64 i = 0; i < size; i += 4096) {
        kresult_t res = map_page(page_table, phys_addr + i, virt_addr + i, arg);
        if (res != SUCCESS) {
            return res;
        }
    }

    // On failure, pages need to be unmapped here...
    // Leak memory for now :)

    return SUCCESS;
}

u64 prepare_leaf_pt_for(void * virt_addr, Page_Table_Argumments /* unused */, u64 pt_ptr) noexcept
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(pt_ptr);
    for (int i = riscv64_paging_levels; i > 1; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (u64(virt_addr) >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];

        u64 next_level_phys;
        if (not entry->valid) {
            // Allocate a new page table
            u64 new_pt_phys;

            // TODO: Contemplate about removing exceptions :)
            try {
                new_pt_phys = kernel_pframe_allocator.alloc_page_ppn();
            } catch (Kern_Exception &e) {
                return e.err_code;
            }

            RISCV64_PTE new_entry = RISCV64_PTE();
            new_entry.valid = true;
            new_entry.ppn = new_pt_phys;
            *entry = new_entry;

            next_level_phys = new_pt_phys << 12;
            clear_page(next_level_phys);
        } else if (entry->is_leaf()) {
            return ERROR_HUGE_PAGE;
        } else {
            next_level_phys = entry->ppn << 12;
        }

        active_pt = mapper.map(next_level_phys);
        if (i == 2) {
            return next_level_phys;
        } 
    }

    // 0 paging levels???
    return ERROR_GENERAL;
}

kresult_t map_kernel_page(u64 phys_addr, void * virt_addr, Page_Table_Argumments arg)
{
    const u64 pt_top = get_current_hart_pt();
    return riscv_map_page(pt_top, phys_addr, virt_addr, arg);
}

kresult_t unmap_kernel_page(void * virt_addr)
{
    const u64 pt_top = get_current_hart_pt();
    return riscv_unmap_page(pt_top, virt_addr);
}

u64 get_current_hart_pt() noexcept
{
    u64 satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    return (satp & ((1UL << 44) - 1)) << 12;
}

void apply_page_table(ptable_top_ptr_t page_table)
{
    u64 ppn = page_table >> 12;
    u64 asid = 0;
    u64 mode = 5 + riscv64_paging_levels;
    u64 satp = (mode << 60) | (asid << 44) | (ppn << 0);

    // Apply the new page table
    asm volatile("csrw satp, %0" : : "r"(satp) : "memory");

    // Flush the TLB
    asm volatile("sfence.vma x0, x0" : : : "memory");
}

