#pragma once
#include <types.hh>

namespace kernel::x86::cpus
{

void apply_io_bitmap(phys_addr_t bitmap_phys, u64 page_table_id, bool do_invlpg = false);

} // namespace kernel::x86::cpus