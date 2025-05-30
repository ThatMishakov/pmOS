#pragma once

#include "loongarch64_paging.hh"

namespace kernel::paging
{
// Generic declaration for arch-independent code
using Arch_Page_Table = kernel::loongarch64::paging::LoongArch64_Page_Table;
} // namespace kernel::paging