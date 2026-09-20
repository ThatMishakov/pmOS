#include "arch_paging.hh"
#include <cpu.hh>
#include <pmos/containers/map.hh>
#include "m68030.hh"

namespace {

void flush_i_d()
{
    asm("movec %%cacr, %%d0\n\t"
        "oriw %0,%%d0\n\t"
        "movec %%d0,%%cacr"
        :
        : "i"(0x808)
        : "d0", "memory");
}

// void flush_tlb()
// {
//     asm("pflusha");
// }

void flush_m68020(void *virt_addr)
{
    register void *a0 asm("%a0") = virt_addr;

    asm(".word 0xf010, 0x0810\n\t" // pflush #7, #7, (A0)
        "nop\n\t"
        :
        : "r"(a0)
        : "memory");
}

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
        flush_m68020(addr);
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
        for (u64 i = 0; i < size; i += PAGE_SIZE)
            flush_m68020((void *)((char *)addr + i));

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

phys_addr_t kernel::paging::arch_phys_addr_limit()
{
    return 0;
}

namespace kernel::paging {

kresult_t map_kernel_page(phys_addr_t phys_addr, void *virt_addr, Page_Table_Arguments arg)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::m68030_map_kernel_page(phys_addr, virt_addr, arg);
    default:
        assert(false);
    }

    return -ENOSYS;
}

kresult_t unmap_kernel_page(kernel::paging::TLBShootdownContext &ctx, void *virt_addr)
{
    switch (kernel::m68k::cpu_kind) {
    case kernel::m68k::CpuKind::M68020:
    case kernel::m68k::CpuKind::M68030:
        return m68k::paging::m68030_unmap_kernel_page(ctx, virt_addr);
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

} // namespace kernel::paging