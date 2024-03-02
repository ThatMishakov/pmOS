#include "riscv64_paging.hh"
#include <memory/temp_mapper.hh>
#include <memory/mem.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <assert.h>
#include <processes/tasks.hh>

void flush_page(void *virt) noexcept
{
    asm volatile("sfence.vma %0, x0" : : "r"(virt) : "memory");
}

void riscv_map_page(u64 pt_top_phys, u64 phys_addr, void * virt_addr, Page_Table_Argumments arg)
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
                throw Kern_Exception(ERROR_PAGE_PRESENT, "Page already present");
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
            return;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                // Allocate a new page table
                u64 new_pt_phys = kernel_pframe_allocator.alloc_page_ppn();

                RISCV64_PTE new_entry = RISCV64_PTE();
                new_entry.valid = true;
                new_entry.ppn = new_pt_phys;
                *entry = new_entry;

                next_level_phys = new_pt_phys << 12;
                clear_page(next_level_phys);
            } else if (entry->is_leaf()) {
                throw Kern_Exception(ERROR_HUGE_PAGE, "Huge page encountered");
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    // 0 paging levels???
    assert(!"0 paging levels");
}

void RISCV64_Page_Table::map(u64 page_addr, u64 virt_addr, Page_Table_Argumments arg)
{
    riscv_map_page(table_root, page_addr, (void*)virt_addr, arg);
}

void RISCV64_Page_Table::map(Page_Descriptor page, u64 virt_addr, Page_Table_Argumments arg)
{
    u64 pte_phys = prepare_leaf_pt_for((void*)virt_addr, arg, table_root);

    const int index = (virt_addr >> 12) & 0x1FF;

    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(pte_phys);

    auto &entry = active_pt[index];
    if (entry.valid)
        throw Kern_Exception(ERROR_PAGE_PRESENT, "Page already present");

    RISCV64_PTE pte = RISCV64_PTE();
    pte.valid = true;
    pte.user = arg.user_access;
    pte.writeable = arg.writeable;
    pte.readable = arg.readable;
    pte.executable = not arg.execution_disabled;
    pte.available = page.owning ? 0 : PAGING_FLAG_NOFREE;
    pte.ppn = page.takeout_page().first >> 12;

    entry = pte;
}

void riscv_unmap_page(u64 pt_top_phys, void * virt_addr)
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

            if (not entry->valid) {
                throw Kern_Exception(ERROR_PAGE_NOT_PRESENT, "Page not present");
            }

            RISCV64_PTE entry = active_pt[index];
            if (not entry.is_special()) {
                // Free the page
                kernel_pframe_allocator.free_ppn(entry.ppn);
            }
            entry = RISCV64_PTE();
            active_pt[index] = entry;
            flush_page(virt_addr);
            return;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                throw Kern_Exception(ERROR_PAGE_NOT_PRESENT, "Page not present");
            } else if (entry->is_leaf()) {
                throw Kern_Exception(ERROR_HUGE_PAGE, "Huge page encountered");
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    assert(!"0 paging levels");
}

void RISCV64_Page_Table::invalidate(u64 virt_addr, bool free) noexcept
{
    bool invalidated = false;
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            if (entry->valid) {
                if (free and not entry->is_special()) {
                    // Free the page
                    kernel_pframe_allocator.free_ppn(entry->ppn);
                }
                *entry = RISCV64_PTE();
                invalidated = true;
            }
            break;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                break;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                break;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    if (invalidated)
        flush_page((void*)virt_addr);
}

bool RISCV64_Page_Table::is_mapped(u64 virt_addr) const noexcept
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            return entry->valid;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                return false;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                return false;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }

    return false;
}

// TODO: This function had great possibilities, but now seems weird
RISCV64_Page_Table::Page_Info RISCV64_Page_Table::get_page_mapping(u64 virt_addr) const
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());

    RISCV64_PTE *active_pt = mapper.map(table_root);
    for (int i = riscv64_paging_levels; i > 0; --i) {
        const u8 offset = 12 + (i-1)*9;
        const u64 index = (virt_addr >> offset) & 0x1FF;

        RISCV64_PTE *entry = &active_pt[index];
        if (i == 1) {
            // Leaf page table
            if (entry->valid) {
                Page_Info i{};
                i.flags = entry->available;
                i.is_allocated = entry->valid;
                i.dirty = entry->dirty;
                i.user_access = entry->user;
                i.page_addr = entry->ppn << 12;
                i.nofree = entry->available & PAGING_FLAG_NOFREE;
                return i;
            }
            break;
        } else {
            // Non-leaf page table
            u64 next_level_phys;
            if (not entry->valid) {
                break;
            } else if (entry->is_leaf()) {
                assert(!"Unexpected huge page!");
                break;
            } else {
                next_level_phys = entry->ppn << 12;
            }

            active_pt = mapper.map(next_level_phys);
        }
    }
    return Page_Info{};
}

void map_page(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, Page_Table_Argumments arg)
{
    riscv_map_page(page_table, phys_addr, (void*)virt_addr, arg);
}

void map_pages(ptable_top_ptr_t page_table, u64 phys_addr, u64 virt_addr, u64 size, Page_Table_Argumments arg)
{
    // Don't overcomplicate things for now, just call map
    // This is *very* inefficient
    for (u64 i = 0; i < size; i += 4096) {
        map_page(page_table, phys_addr + i, virt_addr + i, arg);
    }

    // On failure, pages need to be unmapped here...
    // Leak memory for now :)
}

u64 prepare_leaf_pt_for(void * virt_addr, Page_Table_Argumments /* unused */, u64 pt_ptr)
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
            u64 new_pt_phys = kernel_pframe_allocator.alloc_page_ppn();

            RISCV64_PTE new_entry = RISCV64_PTE();
            new_entry.valid = true;
            new_entry.ppn = new_pt_phys;
            *entry = new_entry;

            next_level_phys = new_pt_phys << 12;
            clear_page(next_level_phys);
        } else if (entry->is_leaf()) {
            throw Kern_Exception(ERROR_HUGE_PAGE, "Huge page encountered");
        } else {
            next_level_phys = entry->ppn << 12;
        }

        active_pt = mapper.map(next_level_phys);
        if (i == 2) {
            return next_level_phys;
        } 
    }

    // 0 paging levels???
    assert(!"0 paging levels");
    return 0;
}

void map_kernel_page(u64 phys_addr, void * virt_addr, Page_Table_Argumments arg)
{
    const u64 pt_top = get_current_hart_pt();
    riscv_map_page(pt_top, phys_addr, virt_addr, arg);
}

void unmap_kernel_page(void * virt_addr)
{
    const u64 pt_top = get_current_hart_pt();
    riscv_unmap_page(pt_top, virt_addr);
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

void RISCV64_Page_Table::invalidate_range(u64 virt_addr, u64 size_bytes, bool free)
{
    // Slow but doesn't matter for now
    u64 end = virt_addr + size_bytes;
    for (u64 i = virt_addr; i < end; i += 4096)
        invalidate(i, free);
}

RISCV64_Page_Table::~RISCV64_Page_Table()
{
    free_user_pages();
    kernel_pframe_allocator.free((void*)table_root);
}

void free_pages_in_level(u64 pt_phys, u64 level)
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(pt_phys);

    for (u64 i = 0; i < 512; ++i) {
        RISCV64_PTE *entry = &active_pt[i];
        if (entry->valid) {
            if (level > 1)
                free_pages_in_level(entry->ppn << 12, level - 1);
            else
                assert(!entry->is_leaf());

            entry->clear_auto();
        }
    }
}


void RISCV64_Page_Table::free_user_pages()
{
    Temp_Mapper_Obj<RISCV64_PTE> mapper(request_temp_mapper());
    RISCV64_PTE *active_pt = mapper.map(table_root);

    for (u64 i = 0; i < 256; ++i) {
        RISCV64_PTE *entry = &active_pt[i];
        if (entry->valid) {
            assert(not entry->is_leaf());
            free_pages_in_level(entry->ppn << 12, riscv64_paging_levels - 1);
            kernel_pframe_allocator.free_ppn(entry->ppn);
        }
    }
}

void RISCV64_PTE::clear_auto()
{
    if (valid and not is_special())
        kernel_pframe_allocator.free_ppn(ppn);

    *this = RISCV64_PTE();
}