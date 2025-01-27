#include "x86_paging.hh"
#include <x86_asm.hh>

void apply_page_table(ptable_top_ptr_t page_table)
{
    setCR3(page_table);
}

void IA32_Page_Table::apply()
{
    apply_page_table(cr3);
}