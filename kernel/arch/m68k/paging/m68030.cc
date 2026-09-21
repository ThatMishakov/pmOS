#include "m68030.hh"
#include <pmos/utility/scope_guard.hh>


static constexpr u32 DESCRIPTOR_INVALID = 0x0;
static constexpr u32 DESCRIPTOR_PAGE = 0x1;
static constexpr u32 DESCRIPTOR_SHORT = 0x2;
static constexpr u32 DESCRIPTOR_LONG = 0x3;

// Short format
static constexpr u32 DESCRIPTOR_TYPE = 0x3;
static constexpr u32 WRITE_PROTECTED = 1 << 2;
static constexpr u32 ACCESS_BIT = 1 << 3;

// Page descriptor
static constexpr u32 MODIFIED_BIT = 1 << 4;
static constexpr u32 CACHE_INHIBIT = 1 << 6;
// Long format
static constexpr u32 SUPERVISOR_ONLY = 1 << 8;

static constexpr u32 TABLE_MASK = 0xfffffff0;
static constexpr u32 PAGE_MASK = 0xffffff00;

static constexpr u32 ACTUAL_MASK = 0xfffff000; // 4KB mask

// Using 2 levels of paging, with 4KB pages, and 4KB page tables

using namespace kernel::paging;

static u32 kernel_page_table = 0;

static bool cache_inhibit(kernel::paging::Page_Table_Arguments arg)
{
    switch (arg.cache_policy) {
    case kernel::paging::Memory_Type::Normal:
    case kernel::paging::Memory_Type::Framebuffer:
        return false;
    case kernel::paging::Memory_Type::MemoryNoCache:
    case kernel::paging::Memory_Type::IONoCache:
        return true;
    }

    assert(false && "Invalid cache policy");
    return false;
}

namespace kernel::m68k::paging
{

kresult_t m68030_map_page(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr,
                   kernel::paging::Page_Table_Arguments arg)
{
    assert(!(phys_addr & ~ACTUAL_MASK));
    assert(!(reinterpret_cast<uintptr_t>(virt_addr) & ~ACTUAL_MASK));

    uintptr_t virt = reinterpret_cast<uintptr_t>(virt_addr);

    kernel::paging::Temp_Mapper_Obj<u32> mapper(kernel::paging::request_temp_mapper());
    auto top = mapper.map(page_table);

    auto a_level = (virt >> 22) & 0x3ff;
    auto b_level = (virt >> 12) & 0x3ff;

    auto a_entry = __atomic_load_n(top + a_level, __ATOMIC_RELAXED);
    if ((a_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_SHORT) {
        auto new_table = pmm::get_memory_for_kernel(1);
        if (pmm::alloc_failure(new_table))
            return -ENOMEM;

        clear_page(new_table);

        a_entry = new_table | DESCRIPTOR_SHORT;
        __atomic_store_n(top + a_level, a_entry, __ATOMIC_RELEASE);
    }

    kernel::paging::Temp_Mapper_Obj<u32> b_mapper(kernel::paging::request_temp_mapper());
    auto b_table = b_mapper.map(a_entry & TABLE_MASK);
    auto b_entry = __atomic_load_n(b_table + b_level, __ATOMIC_RELAXED);
    if ((b_entry & DESCRIPTOR_TYPE) == DESCRIPTOR_PAGE) {
        return -EEXIST;
    }

    b_entry = phys_addr & PAGE_MASK;
    b_entry |= DESCRIPTOR_PAGE;
    if (!arg.writeable)
        b_entry |= WRITE_PROTECTED;
    if (cache_inhibit(arg))
        b_entry |= CACHE_INHIBIT;
    
    __atomic_store_n(b_table + b_level, b_entry, __ATOMIC_RELEASE);
    return 0;
}

kresult_t m68030_unmap_page(ptable_top_ptr_t page_table, kernel::paging::TLBShootdownContext &ctx, void *virt_addr, bool free)
{
    assert(!(reinterpret_cast<uintptr_t>(virt_addr) & ~ACTUAL_MASK));

    uintptr_t virt = reinterpret_cast<uintptr_t>(virt_addr);

    kernel::paging::Temp_Mapper_Obj<u32> mapper(kernel::paging::request_temp_mapper());
    auto top = mapper.map(page_table);

    auto a_level = (virt >> 22) & 0x3ff;
    auto b_level = (virt >> 12) & 0x3ff;

    auto a_entry = __atomic_load_n(top + a_level, __ATOMIC_RELAXED);
    if ((a_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_SHORT) {
        return -ENOENT;
    }

    kernel::paging::Temp_Mapper_Obj<u32> b_mapper(kernel::paging::request_temp_mapper());
    auto b_table = b_mapper.map(a_entry & TABLE_MASK);
    auto b_entry = __atomic_load_n(b_table + b_level, __ATOMIC_RELAXED);
    if ((b_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_PAGE) {
        return -ENOENT;
    }

    __atomic_store_n(b_table + b_level, 0, __ATOMIC_RELEASE);
    ctx.invalidate_page(virt_addr);
    if (free)
        pmm::free_memory_for_kernel(b_entry & PAGE_MASK, 1);

    return 0;
}

kresult_t m68030_unmap_kernel_page(kernel::paging::TLBShootdownContext &ctx, void *virt_addr, bool free)
{
    return m68030_unmap_page(kernel_page_table, ctx, virt_addr, free);
}

u32 m68030_kernel_page_table()
{
    return kernel_page_table;
}

klib::shared_ptr<M68030PageTable> M68030PageTable::create_empty(unsigned flags)
{
    klib::shared_ptr<M68030PageTable> table = klib::unique_ptr<M68030PageTable>(new M68030PageTable());
    if (!table)
        return {};

    auto phys = pmm::get_memory_for_kernel(1);
    if (pmm::alloc_failure(phys))
        return {};
    clear_page(phys);

    auto guard = pmos::utility::make_scope_guard([&]() {
        pmm::free_memory_for_kernel(phys, 1);
    });

    auto r = insert_global_page_tables(table);
    if (r)
        return {};

    table->table_root = phys;
    guard.dismiss();
    return table;
}

klib::shared_ptr<Page_Table> M68030PageTable::create_clone()
{
    // TODO
    return {};
}

} // namespace kernel::m68k::paging
