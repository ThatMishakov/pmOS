#pragma once
#include <memory/paging.hh>

class IA32_Page_Table: public Page_Table
{
public:
    virtual klib::shared_ptr<IA32_Page_Table> create_clone();
    u64 user_addr_max() const override;

    static klib::shared_ptr<IA32_Page_Table> get_page_table(u64 id);
    static bool is_32bit() { return true; }

    static klib::shared_ptr<IA32_Page_Table> create_empty(unsigned flags = 0);

    void apply();

    virtual ~IA32_Page_Table();
protected:
    unsigned cr3 = 0;
};
