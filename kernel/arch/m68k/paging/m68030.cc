#include "m68030.hh"
#include <pmos/utility/scope_guard.hh>
#include <algorithm>
#include <asm.hh>


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

kresult_t M68030PageTable::map(phys_addr_t page_addr, void *virt_addr, Page_Table_Arguments arg)
{
    return m68030_map_page(table_root, page_addr, virt_addr, arg);
}

template <typename T>
static T alignup(T value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void m68030_invalidate_range(phys_addr_t page_table, kernel::paging::TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free)
{
    assert(!(reinterpret_cast<uintptr_t>(virt_addr) & ~ACTUAL_MASK));
    assert(!(size_bytes & ~ACTUAL_MASK));

    u32 limit        = u32(virt_addr) + size_bytes;
    u32 first_a_idx = (u32(virt_addr) >> 22) & 0x3FF;
    u32 end_aligned  = alignup((u32(virt_addr) + size_bytes), 22);
    u32 last_idx     = (end_aligned >> 22) & 0x3FF;
    if (last_idx == 0 && end_aligned != (u32)virt_addr)
        last_idx = 1024;

    kernel::paging::Temp_Mapper_Obj<u32> mapper(kernel::paging::request_temp_mapper());
    auto top = mapper.map(page_table);

    for (u32 i = first_a_idx; i < last_idx; ++i) {
        auto a_entry = __atomic_load_n(top + i, __ATOMIC_RELAXED);
        if ((a_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_SHORT) {
            continue;
        }

        kernel::paging::Temp_Mapper_Obj<u32> b_mapper(kernel::paging::request_temp_mapper());
        auto b_table = b_mapper.map(a_entry & TABLE_MASK);

        i32 a_addr = (i << 22);
        u32 start_idx = a_addr > (u32)virt_addr ? 0 : ((u32)virt_addr >> 12) & 0x3FF;
        u32 end_idx = (limit == 0 && virt_addr != nullptr) or
                      (last_idx == 1024 and i != 1023) or
                      ((limit >= 0x400000) and (a_addr >= limit - 0x400000))
                      ? 1024
                      : (limit >> 12) & 0x3FF;

        for (unsigned j = start_idx; j < end_idx; ++j) {
            auto b_entry = __atomic_load_n(b_table + j, __ATOMIC_RELAXED);
            if ((b_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_PAGE) {
                continue;
            }

            __atomic_store_n(b_table + j, 0, __ATOMIC_RELEASE);
            ctx.invalidate_page((void *)(a_addr + (j << 12)));
            if (free)
                pmm::free_memory_for_kernel(b_entry & PAGE_MASK, 1);
        }
    }
}

void M68030PageTable::invalidate_range(kernel::paging::TLBShootdownContext &ctx, void *virt_addr, size_t size_bytes,
                                  bool free)
{
    m68030_invalidate_range(table_root, ctx, virt_addr, size_bytes, free);
}

void M68030PageTable::invalidate_tlb(void *page)
{
    m68030_flush_user_page(page);
    flush_i_d();
}

void M68030PageTable::invalidate_tlb(void *start, size_t size)
{
    for (phys_addr_t i = 0; i < size; i += PAGE_SIZE)
        m68030_flush_user_page((void *)((char *)start + i));
    flush_i_d();
}

void M68030PageTable::tlb_flush_all()
{
    m68030_flush_user_all();
    flush_i_d();
}

Page_Info M68030PageTable::get_page_mapping(void *virt_addr) const
{
    assert(!(reinterpret_cast<uintptr_t>(virt_addr) & ~ACTUAL_MASK));

    uintptr_t virt = reinterpret_cast<uintptr_t>(virt_addr);

    kernel::paging::Temp_Mapper_Obj<u32> mapper(kernel::paging::request_temp_mapper());
    auto top = mapper.map(table_root);

    auto a_level = (virt >> 22) & 0x3ff;
    auto b_level = (virt >> 12) & 0x3ff;

    auto a_entry = __atomic_load_n(top + a_level, __ATOMIC_RELAXED);
    if ((a_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_SHORT) {
        return {};
    }

    kernel::paging::Temp_Mapper_Obj<u32> b_mapper(kernel::paging::request_temp_mapper());
    auto b_table = b_mapper.map(a_entry & TABLE_MASK);
    auto b_entry = __atomic_load_n(b_table + b_level, __ATOMIC_RELAXED);
    if ((b_entry & DESCRIPTOR_TYPE) != DESCRIPTOR_PAGE) {
        return {};
    }

    Page_Info info{};
    info.page_addr = b_entry & PAGE_MASK;
    info.writeable = !(b_entry & WRITE_PROTECTED);
    info.cache_policy =
        (b_entry & CACHE_INHIBIT) ? kernel::paging::Memory_Type::MemoryNoCache : kernel::paging::Memory_Type::Normal;
    info.user_access = true;
    return info;
}

void M68030PageTable::apply()
{
    u64 root_pointer = table_root;
    root_pointer |= (u64)DESCRIPTOR_SHORT << 32; 
    root_pointer |= (u64)1024 << 48;

    asm("pmove %0, %%crp" :: "m"(root_pointer) : "memory" );
}

} // namespace kernel::m68k::paging
