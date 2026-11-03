#pragma once

#include "aarch64_paging.hh"

namespace kernel::paging
{
// Generic declaration for arch-independent code
using Arch_Page_Table = kernel::aarch64::paging::AArch64_Page_Table;
} // namespace kernel::paging