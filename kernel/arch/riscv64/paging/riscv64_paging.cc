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
    return (satp & (((1UL << 44) - 1))) << 12;
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

void RISCV64_Page_Table::takeout_global_page_tables()
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.erase_if_exists(this->id);
}

void RISCV64_Page_Table::insert_global_page_tables(klib::shared_ptr<RISCV64_Page_Table> table)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    global_page_tables.insert({table->id, table});
}

RISCV64_Page_Table::page_table_map RISCV64_Page_Table::global_page_tables;
Spinlock RISCV64_Page_Table::page_table_index_lock;

void RISCV64_Page_Table::atomic_active_sum(u64 i) noexcept
{
    __atomic_add_fetch(&active_counter, i, __ATOMIC_SEQ_CST);
}

void RISCV64_Page_Table::apply() noexcept
{
    apply_page_table(table_root);
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::get_page_table(u64 id) noexcept
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    return global_page_tables.get_copy_or_default(id).lock();
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::get_page_table_throw(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);

    try {
        return global_page_tables.at(id).lock();
    } catch (const std::out_of_range&) {
        throw Kern_Exception(ERROR_OBJECT_DOESNT_EXIST, "requested page table doesn't exist");
    }
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::create_clone() {
    // Not implemented
    throw Kern_Exception(ERROR_NOT_IMPLEMENTED, "Cloning RISC-V page tables is not yet implemented");
}

klib::shared_ptr<RISCV64_Page_Table> RISCV64_Page_Table::capture_initial(u64 root) {
    klib::shared_ptr<RISCV64_Page_Table> new_table = klib::unique_ptr<RISCV64_Page_Table>(new RISCV64_Page_Table());

    new_table->table_root = root;
    insert_global_page_tables(new_table);

    return new_table;
}

u64 RISCV64_Page_Table::user_addr_max() const noexcept
{
    // Of the page tables, user space gets 256 entries in the root level + other levels
    // For example, in RV39, with 3 levels of paging, the following is the pointer breakdown:
    // | all 0 | 0XXXXXXXX 256 entries (8 bits) | XXXXXXXXX 512 entries (9 bits) | XXXXXXXXX 512 entries (9 bits) | 4096 bytes in a page, 12 bits |
    // 12 bits + levels * 9 bits - 1 bit (half of virtual memory is used by kernel)
    return 1UL << (12 + (riscv64_paging_levels * 9) - 1);
}