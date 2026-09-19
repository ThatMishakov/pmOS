#pragma once

#include <memory/paging.hh>

namespace kernel::m68k
{

class Page_Table : public kernel::paging::Page_Table
{
public:
    constexpr static bool is_32bit() { return true; }
    virtual void apply() = 0;

    static klib::shared_ptr<Page_Table> create_empty(unsigned flags = 0);
    virtual klib::shared_ptr<Page_Table> create_clone() = 0;

    // Hides kernel::paging::Page_Table::get_page_table so generic code that
    // mixes Arch_Page_Table and base holders (e.g. `cond ? arch_ptr :
    // Arch_Page_Table::get_page_table(...)`) sees a single type. Like the
    // other archs, this is only declared here; it is defined together with
    // the rest of the m68k paging implementation.
    static klib::shared_ptr<Page_Table> get_page_table(u64 id);
protected:
    static result_t insert_global_page_tables(klib::shared_ptr<Page_Table> table);
    void takeout_global_page_tables();
};

} // namespace kernel::m68k

namespace kernel::paging {

using Arch_Page_Table = kernel::m68k::Page_Table;

} // namespace kernel::paging