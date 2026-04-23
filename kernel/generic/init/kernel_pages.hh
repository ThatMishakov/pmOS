#pragma once
#include <types.hh>

namespace kernel {

void map_kernel_pages(kernel::paging::ptable_top_ptr_t kernel_pt_top, phys_addr_t kernel_phys_base);
void init_pmm(kernel::paging::ptable_top_ptr_t kernel_pt_top);

}