#pragma once

#include "x86_paging.hh"

namespace kernel::paging
{
using Arch_Page_Table = kernel::ia32::paging::IA32_Page_Table;
}