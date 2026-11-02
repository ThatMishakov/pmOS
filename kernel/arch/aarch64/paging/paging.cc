#include "aarch64_paging.hh"
#include <types.hh>

using namespace kernel::aarch64::paging;

u64 max_phys_addr = 0;

phys_addr_t kernel::paging::arch_phys_addr_limit()
{
    return max_phys_addr;
}

bool AArch64_Page_Table::is_32bit() const
{
    return flags & FLAG_32BIT;
}