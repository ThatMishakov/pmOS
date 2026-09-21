#include "arch_paging.hh"
#include <cpu.hh>
#include <pmos/containers/map.hh>
#include "m68030.hh"
#include <asm.hh>

namespace {

// void flush_tlb()
// {
//     asm("pflusha");
// }

void flush_m68040(void *virt_addr)
{
    register void *a0 asm("%a0") = virt_addr;

    asm(".word 0xf508\n\t" // pflush (A0)
        "nop\n\t"
        :
        : "r"(a0)
        : "memory");
}

}

void kernel::paging::invalidate_tlb_kernel(void *addr)
{   
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        flush_m68030(addr);
        flush_i_d();
        break;
    default:
        assert(false);
    }
}

void kernel::paging::invalidate_tlb_kernel(void *addr, size_t size)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        for (phys_addr_t i = 0; i < size; i += PAGE_SIZE)
            flush_m68030((void *)((char *)addr + i));

        flush_i_d();
        break;
    default:
        assert(false);
    }
}

namespace {

pmos::containers::map<u64, klib::weak_ptr<kernel::m68k::Page_Table>> global_page_tables;
Spinlock page_table_index_lock;

}

klib::shared_ptr<kernel::m68k::Page_Table> kernel::m68k::Page_Table::get_page_table(u64 id)
{
    Auto_Lock_Scope scope_lock(page_table_index_lock);
    auto it = global_page_tables.find(id);
    if (it == global_page_tables.end())
        return nullptr;
    return it->second.lock();
}

result_t kernel::m68k::Page_Table::insert_global_page_tables(klib::shared_ptr<Page_Table> table)
{
    Auto_Lock_Scope local_lock(page_table_index_lock);
    auto ret = global_page_tables.insert_noexcept({table->id, table});
    if (ret.first == global_page_tables.end())
        return -ENOMEM;

    return 0;
}

phys_addr_t kernel::paging::arch_phys_addr_limit()
{
    return 0;
}

klib::shared_ptr<kernel::m68k::Page_Table> kernel::m68k::Page_Table::create_empty(unsigned flags)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::M68030PageTable::create_empty(flags);
    default:
        assert(false);
    };

    return {};
}

void *kernel::m68k::Page_Table::user_addr_max() const
{
    return 0;
}

namespace kernel::paging {

kresult_t map_kernel_page(phys_addr_t phys_addr, void *virt_addr, Page_Table_Arguments arg)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::m68030_map_page(kernel::m68k::paging::m68030_kernel_page_table(), phys_addr, virt_addr, arg);
    default:
        assert(false);
    }

    return -ENOSYS;
}

kresult_t unmap_kernel_page(kernel::paging::TLBShootdownContext &ctx, void *virt_addr, bool free)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::m68030_unmap_kernel_page(ctx, virt_addr, free);
    default:
        assert(false);
    }

    return -ENOSYS;
}

kresult_t map_page(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr,
                   Page_Table_Arguments arg)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::m68030_map_page(page_table, phys_addr, virt_addr, arg);
    default:
        assert(false);
    }

    return -ENOSYS;
}

kresult_t map_pages(ptable_top_ptr_t page_table, phys_addr_t phys_addr, void *virt_addr, size_t size,
                    kernel::paging::Page_Table_Arguments arg)
{
    for (phys_addr_t i = 0; i < size; i += PAGE_SIZE) {
        phys_addr_t current_phys_addr = phys_addr + i;
        void *current_virt_addr = (void *)((char *)virt_addr + i);
        auto result = map_page(page_table, current_phys_addr, current_virt_addr, arg);
        if (result)
            return result;
    }
    return 0;
}

kresult_t map_kernel_pages(phys_addr_t phys_addr, void *virt_addr, size_t size, Page_Table_Arguments arg)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return map_pages(kernel::m68k::paging::m68030_kernel_page_table(), phys_addr, virt_addr, size, arg);
    default:
        assert(false);
    }

    return -ENOSYS;
}

} // namespace kernel::paging