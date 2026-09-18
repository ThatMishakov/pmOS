#pragma once

#include <memory/paging.hh>

namespace kernel::m68k
{

class Page_Table : public kernel::paging::Page_Table
{
public:
    constexpr static bool is_32bit() { return true; }
    void apply();

    static klib::shared_ptr<Page_Table> create_empty(unsigned flags = 0);
    klib::shared_ptr<Page_Table> create_clone();

    // Hides kernel::paging::Page_Table::get_page_table so generic code that
    // mixes Arch_Page_Table and base holders (e.g. `cond ? arch_ptr :
    // Arch_Page_Table::get_page_table(...)`) sees a single type. Like the
    // other archs, this is only declared here; it is defined together with
    // the rest of the m68k paging implementation.
    static klib::shared_ptr<Page_Table> get_page_table(u64 id);

private:
    u32 pt;
};

} // namespace kernel::m68k

namespace kernel::paging {

using Arch_Page_Table = kernel::m68k::Page_Table;

} // namespace kernel::paging